#include <components/files/hash.hpp>
#include <components/files/constrainedfilestream.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    using namespace testing;
    using namespace Files;

    struct Params
    {
        std::size_t mSize;
        std::array<std::uint64_t, 2> mHash;
    };

    struct FilesGetHash : TestWithParam<Params> {};

    TEST_P(FilesGetHash, shouldReturnHashForStringStream)
    {
        const std::string fileName = "fileName";
        std::string content;
        std::fill_n(std::back_inserter(content), GetParam().mSize, 'a');
        std::istringstream stream(content);
        EXPECT_EQ(getHash(fileName, stream), GetParam().mHash);
    }

    TEST_P(FilesGetHash, shouldReturnHashForConstrainedFileStream)
    {
        std::string fileName(UnitTest::GetInstance()->current_test_info()->name());
        std::replace(fileName.begin(), fileName.end(), '/', '_');
        std::string content;
        std::fill_n(std::back_inserter(content), GetParam().mSize, 'a');
        std::fstream(fileName, std::ios_base::out | std::ios_base::binary)
            .write(content.data(), static_cast<std::streamsize>(content.size()));
        const auto stream = Files::openConstrainedFileStream(fileName.data(), 0, content.size());
        EXPECT_EQ(getHash(fileName, *stream), GetParam().mHash);
    }

    INSTANTIATE_TEST_SUITE_P(Params, FilesGetHash, Values(
        Params {0, {0, 0}},
        Params {1, {9607679276477937801ull, 16624257681780017498ull}},
        Params {128, {15287858148353394424ull, 16818615825966581310ull}},
        Params {1000, {11018119256083894017ull, 6631144854802791578ull}},
        Params {4096, {11972283295181039100ull, 16027670129106775155ull}},
        Params {4097, {16717956291025443060ull, 12856404199748778153ull}},
        Params {5000, {15775925571142117787ull, 10322955217889622896ull}}
    ));
}
