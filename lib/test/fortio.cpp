#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <iostream>

#include <catch/catch.hpp>

#include <ecl/fortio.h>

#include "matchers.hpp"

namespace {

struct fcloser {
    void operator()( std::FILE* fp ) { fclose( fp ); }
};

using ufile = std::unique_ptr< std::FILE, fcloser >;

}

using Catch::Matchers::Equals;

void write_record( std::FILE* fp,
                   const int32_t head,
                   std::vector < int32_t > src ) {

    auto elems = std::fwrite( &head, sizeof(head), 1, fp );
    REQUIRE_THAT( elems, FwriteCount( 1 ) );

    elems = std::fwrite( src.data(), sizeof(head), src.size(), fp );
    REQUIRE_THAT( elems, FwriteCount( src.size()) );

    elems = std::fwrite( &head, sizeof(head), 1, fp);
    REQUIRE_THAT( elems, FwriteCount( 1 ) );
}

TEST_CASE("valid records can be read", "[fortio][f77]") {
    ufile handle( std::tmpfile() );
    REQUIRE( handle );
    auto* fp = handle.get();

    SECTION( "type of int32" ) {
        std::vector< std::int32_t > src ( 10, 0 );

        std::int32_t size = src.size();
        const std::int32_t head = sizeof( std::int32_t ) * size;

        std::iota(src.begin(), src.end(), 0);

        write_record( fp, head, src);
        auto out = src;

        Err err = eclfio_get( fp, "e", &size, out.data() );
        CHECK( err == Err::ok() );
        CHECK( src == out );
    }

    SECTION( "type of char" ) {
        std::vector< char > src ( 10, 'A' );

        std::int32_t size = src.size();
        const std::int32_t head = sizeof( std::int32_t ) * size;

        auto elems = std::fwrite( &head, sizeof(head), 1, fp );
        REQUIRE_THAT( elems, FwriteCount( 1 ) );

        elems = std::fwrite( src.data(), sizeof(head), src.size(), fp );
        REQUIRE_THAT( elems, FwriteCount( src.size()) );

        elems = std::fwrite( &head, sizeof(head), 1, fp);
        REQUIRE_THAT( elems, FwriteCount( 1 ) );
        auto out = src;

        Err err = eclfio_get( fp, "e", &size, out.data() );
        CHECK( err == Err::ok() );
        CHECK( src == out );

    }
    SECTION( "type of charn" ) {

    }
}

TEST_CASE("records with broken tail can be read", "[fortio][f77]") {
    ufile handle( std::tmpfile() );
    REQUIRE( handle );
    auto* fp = handle.get();

    std::vector< std::int32_t > src( 10, 0 );
    const std::int32_t head = sizeof( std::int32_t ) * src.size();

    for( int i = 0; i < int(src.size()); ++i )
        src[ i ] = i;

    auto elems = std::fwrite( &head, sizeof( head ), 1, fp );
    REQUIRE_THAT( elems, FwriteCount( 1 ) );

    elems = std::fwrite( src.data(), sizeof( head ), src.size(), fp );
    REQUIRE_THAT( elems, FwriteCount( src.size() ) );

    std::int32_t size = src.size();
    
    SECTION( "missing tail" ) {

        SECTION("querying size is not affected") {
            std::rewind( fp );

            Err err = eclfio_sizeof( fp, "e", &size );
            CHECK( err == Err::ok() );
            CHECK( size == 10 );
        }

        SECTION("failure with strict read") {
            std::rewind( fp );
            auto out = src;
            const auto pos = std::ftell( fp );

            Err err = eclfio_get( fp, "e", &size, out.data() );
            CHECK( err == Err::unexpected_eof() );
            CHECK( size == 10 );
            CHECK( pos == std::ftell( fp ) );
        }

        SECTION("success with allow-notail ($)") {
            std::rewind( fp );
            auto out = src;
            const auto pos = std::ftell( fp );

            Err err = eclfio_get( fp, "e$", &size, out.data() );
            CHECK( err == Err::ok() );
            CHECK( size == 10 );
            CHECK( pos < std::ftell( fp ) );
            CHECK_THAT( out, Equals( src ) );
        }

        SECTION("success with force-notail (~)") {
            std::rewind( fp );
            auto out = src;
            const auto pos = std::ftell( fp );

            Err err = eclfio_get( fp, "e~", &size, out.data() );
            CHECK( err == Err::ok() );
            CHECK( size == 10 );
            CHECK( pos < std::ftell( fp ) );
            CHECK_THAT( out, Equals( src ) );
        }
    }

    SECTION( "mismatching between head and tail" ) {
        auto tail = head + 1;
        auto elems = std::fwrite( &tail, sizeof( head ), 1, fp );
        REQUIRE_THAT( elems, FwriteCount( 1 ) );

        SECTION("querying size is not affected") {
            std::rewind( fp );

            Err err = eclfio_sizeof( fp, "e", &size );
            CHECK( err == Err::ok() );
            CHECK( size == 10 );
        }

        SECTION("failure with strict read") {
            std::rewind( fp );
            auto out = src;
            const auto pos = std::ftell( fp );

            Err err = eclfio_get( fp, "e", &size, out.data() );
            CHECK( err == Err::invalid_record() );
            CHECK( size == 10 );
            CHECK( pos == std::ftell( fp ) );
        }

        SECTION("success with allow-notail ($)") {
            std::rewind( fp );
            auto out = src;
            const auto pos = std::ftell( fp );

            Err err = eclfio_get( fp, "e$", &size, out.data() );
            CHECK( err == Err::ok() );
            CHECK( size == 10 );
            CHECK( pos < std::ftell( fp ) );
            CHECK_THAT( out, Equals( src ) );
        }

        SECTION("success with force-notail (~)") {
            std::rewind( fp );
            auto out = src;
            const auto pos = std::ftell( fp );

            Err err = eclfio_get( fp, "e~", &size, out.data() );
            CHECK( err == Err::ok() );
            CHECK( size == 10 );
            CHECK( pos < std::ftell( fp ) );
            CHECK_THAT( out, Equals( src ) );
        }
    }
}

TEST_CASE("record with valid, but too small body", "[fortio][f77]") {
    ufile handle( std::tmpfile() );
    REQUIRE( handle );
    auto* fp = handle.get();

    std::vector< std::int32_t > src( 10, 0 );
    const std::int32_t head = sizeof(std::int32_t) * (src.size() + 2) ;

    auto elems = std::fwrite( &head, sizeof( head ), 1, fp );
    REQUIRE_THAT( elems, FwriteCount( 1 ) );

    elems = std::fwrite( src.data(), sizeof( head ), src.size(), fp );
    REQUIRE_THAT( elems, FwriteCount( src.size() ) );

    std::rewind( fp );
    const auto pos = std::ftell( fp );
    std::vector< std::int32_t > out( head, 0 );
    std::int32_t size = out.size();
    Err err = eclfio_get( fp, "e", &size, out.data() );

    CHECK( err == Err::unexpected_eof() );
    CHECK( pos == std::ftell( fp ) );
    CHECK( size == out.size() );
}

TEST_CASE("record with invalid head", "[fortio][f77]") {
    ufile handle( std::tmpfile() );
    REQUIRE( handle );
    auto* fp = handle.get();

    std::vector< std::int32_t > src( 10, 0 );

    for( std::int32_t head : { -4, 11 } ) SECTION(
          head < 0
        ? "negative length"
        : "length not multiple of element size" ) {

        auto elems = std::fwrite( &head, sizeof( head ), 1, fp );
        REQUIRE_THAT( elems, FwriteCount( 1 ) );

        const std::vector< std::int32_t > src( 10, 0 );
        elems = std::fwrite( src.data(), sizeof( head ), src.size(), fp );
        REQUIRE_THAT( elems, FwriteCount( src.size() ) );

        std::rewind( fp );
        const auto pos = std::ftell( fp );
        std::int32_t size = src.size();
        Err err = eclfio_get( fp, "e", nullptr, nullptr );

        CHECK( err == Err::invalid_record() );
        CHECK( pos == std::ftell( fp ) );
        CHECK( size == src.size() );
    }
}

TEST_CASE("requesting string does not consider endianness", "[fortio][f77]") {
    ufile handle( std::tmpfile() );
    REQUIRE( handle );
    auto* fp = handle.get();

    const std::string expected = "FOPT    MINISTEP";
    Err err = eclfio_put( fp, "b", 16, expected.data() );
    REQUIRE( err == Err::ok() );
    std::rewind( fp );

    for( const std::string opts : { "s", "st", "ts", "fst" } )
    SECTION( "with options: " + opts ) {
        char data[17]  = { 0 };
        std::int32_t size = 2;
        err = eclfio_get( fp, opts.c_str(), &size, data );

        CHECK( err == Err::ok() );
        CHECK( data == expected );
        CHECK( size == 2 );
    }
}

TEST_CASE("valid arrays with eclfio_array_get","[fortio][f77]") {
    std::FILE* fp = std::tmpfile();

    std::vector< std::int32_t > src ( 10, 0 );
    std::vector< std::int32_t > out ( 10 );

    const std::int32_t size = src.size();
    const std::int32_t head = sizeof( std::int32_t ) * size;

    std::iota(src.begin(), src.end(), 0);

    SECTION("single block") {
        write_record (fp, head, src);
        rewind(fp);

        Err err = eclfio_array_get( fp, "e", 1, int(src.size()), int(src.size()), out.data() );
        CHECK( err == Err::ok() );
        CHECK( out == src);
    }

    SECTION("multiple blocks") {
        write_record ( fp, head, src);
        write_record ( fp, head, src);
        rewind(fp);

        std::vector < std::int32_t > multi_src = src;
        multi_src.insert( multi_src.end(), src.begin(), src.end() );

        out.resize(size*2);
        Err err = eclfio_array_get(fp, "e", 1, size*2, size, out.data());
        CHECK( err == Err::ok() );
        CHECK( out == multi_src);
    }
}

TEST_CASE("invalid array with eclfio_array_get", "[fortio][f77])"){
    std::FILE* fp = std::tmpfile();

    std::vector< std::int32_t > src ( 10, 0 );
    std::vector< std::int32_t > out ( 10 );

    const std::int32_t size = src.size();
    const std::int32_t head = sizeof( std::int32_t ) * src.size();

    std::iota(src.begin(), src.end(), 0);
    write_record (fp, head, src);

    SECTION("last array contains to few elements"){
        std::vector<std::int32_t > wrong_src ( 5, 0 );
        std::iota( wrong_src.begin(), wrong_src.end(), 0 );

        write_record ( fp, head, wrong_src );

        out.resize( size * 2 );
        rewind(fp);
        Err err = eclfio_array_get(fp, "e", 1, size*2, size, out.data());
        CHECK( err == Err::unexpected_eof() );
    }

    SECTION("middle array contains to few elements"){
        std::vector<std::int32_t> wrong_src ( 5, 0 );
        std::iota( wrong_src.begin(), wrong_src.end(), 0 );
        const std::int32_t wrong_head = sizeof( std::int32_t ) * wrong_src.size();

        write_record ( fp, wrong_head, wrong_src );
        write_record ( fp, head, src );

        out.resize( size * 3 );
        rewind(fp);
        Err err = eclfio_array_get(fp, "e", 1, size*3, size, out.data());
        CHECK( err == Err::unaligned_array() );
        CHECK( out == wrong_src);
    }

     SECTION("last array contains to few elements"){
        std::vector<std::int32_t> wrong_src ( 5, 0 );
        std::iota( wrong_src.begin(), wrong_src.end(), 0 );

        write_record ( fp, head, wrong_src );

        out.resize( size * 2 );
        rewind(fp);
        Err err = eclfio_array_get(fp, "e", 1, size*2, size, out.data());
        CHECK( err == Err::truncated() );
    }

    SECTION("mixed types during writing"){
        std::vector< char > inconsistent_type ( 5, 0 );
        const std::int32_t incons_head = sizeof( char ) * inconsistent_type.size();

        auto elems = std::fwrite( &incons_head, sizeof(incons_head), 1, fp );
        REQUIRE_THAT( elems, FwriteCount( 1 ) );

        elems = std::fwrite( src.data(), sizeof(incons_head), src.size(), fp );
        REQUIRE_THAT( elems, FwriteCount( src.size()) );

        elems = std::fwrite( &head, sizeof(incons_head), 1, fp);
        REQUIRE_THAT( elems, FwriteCount( 1 ) );

        out.resize( size * 2 );
        rewind(fp);
        Err err = eclfio_array_get(fp, "e", 1, size*2, size, out.data());
        CHECK( err == Err::truncated() );
    }

    SECTION("inconsistent number of elements in array"){
        std::vector<std::int32_t> wrong_src ( 5, 0 );
        std::iota( wrong_src.begin(), wrong_src.end(), 0 );

        write_record ( fp, head, wrong_src );

        out.resize( size * 2 );
        rewind(fp);
        Err err = eclfio_array_get(fp, "e", 1, size*2, size, out.data());
        CHECK( err == Err::truncated() );
    }
}
