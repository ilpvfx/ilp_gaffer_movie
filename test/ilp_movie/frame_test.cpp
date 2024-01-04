#include <catch2/catch_test_macros.hpp>

#include "ilp_movie/frame.hpp"

namespace {

TEST_CASE("GetBufferSize")
{
  constexpr int w = 64;
  constexpr int h = 48;

  SECTION("rgb")
  {
    const char *pix_fmt_name = ilp_movie::PixFmt::kRGB_P_F32;
    const int p = ilp_movie::CountPlanes(pix_fmt_name).value();
    REQUIRE(p == 3);

    const std::size_t expected_buf_size = static_cast<std::size_t>(p) * static_cast<std::size_t>(w)
                                          * static_cast<std::size_t>(h) * sizeof(float);

    const std::size_t get_buf_size = ilp_movie::GetBufferSize(pix_fmt_name, w, h).value();
    REQUIRE(get_buf_size == expected_buf_size);
  }

  SECTION("rgba")
  {
    const char *pix_fmt_name = ilp_movie::PixFmt::kRGBA_P_F32;
    const int p = ilp_movie::CountPlanes(pix_fmt_name).value();
    REQUIRE(p == 4);

    const std::size_t expected_buf_size = static_cast<std::size_t>(p) * static_cast<std::size_t>(w)
                                          * static_cast<std::size_t>(h) * sizeof(float);

    const std::size_t get_buf_size = ilp_movie::GetBufferSize(pix_fmt_name, w, h).value();
    REQUIRE(get_buf_size == expected_buf_size);
  }

  SECTION("fail")
  {
    REQUIRE(!ilp_movie::GetBufferSize("not_a_pix_fmt_name", w, h).has_value());
    REQUIRE(!ilp_movie::GetBufferSize(ilp_movie::PixFmt::kRGB_P_F32, -1, h).has_value());
    REQUIRE(!ilp_movie::GetBufferSize(ilp_movie::PixFmt::kRGB_P_F32, w, -1).has_value());
  }
}

TEST_CASE("CountPlanes")
{
  REQUIRE(ilp_movie::CountPlanes(ilp_movie::PixFmt::kRGB_P_F32).value() == 3);
  REQUIRE(ilp_movie::CountPlanes(ilp_movie::PixFmt::kRGBA_P_F32).value() == 4);
}

TEST_CASE("FillArrays")
{
  ilp_movie::Frame f{};
  f.hdr.width = 64;
  f.hdr.height = 48;

  REQUIRE(f.data[0] == nullptr);
  REQUIRE(f.data[1] == nullptr);
  REQUIRE(f.data[2] == nullptr);
  REQUIRE(f.data[3] == nullptr);
  REQUIRE(f.linesize[0] == 0);
  REQUIRE(f.linesize[1] == 0);
  REQUIRE(f.linesize[2] == 0);
  REQUIRE(f.linesize[3] == 0);

  SECTION("rgb")
  {
    f.hdr.pix_fmt_name = ilp_movie::PixFmt::kRGB_P_F32;
    f.buf = std::make_unique<uint8_t[]>(// NOLINT
      ilp_movie::GetBufferSize(f.hdr.pix_fmt_name, f.hdr.width, f.hdr.height).value());
    REQUIRE(ilp_movie::FillArrays(/*out*/ f.data,
      /*out*/ f.linesize,
      f.buf.get(),
      f.hdr.pix_fmt_name,
      f.hdr.width,
      f.hdr.height));

    const auto w = static_cast<std::size_t>(f.hdr.width);
    const auto h = static_cast<std::size_t>(f.hdr.height);
    REQUIRE(f.linesize[0] == static_cast<int>(w * sizeof(float)));
    REQUIRE(f.linesize[1] == static_cast<int>(w * sizeof(float)));
    REQUIRE(f.linesize[2] == static_cast<int>(w * sizeof(float)));
    REQUIRE(f.linesize[3] == 0);

    const auto line0 = static_cast<std::size_t>(f.linesize[0]);
    const auto line1 = static_cast<std::size_t>(f.linesize[1]);
    REQUIRE(f.data[0] == f.buf.get());
    REQUIRE(f.data[1] == f.buf.get() + h * line0);
    REQUIRE(f.data[2] == f.buf.get() + h * (line0 + line1));
    REQUIRE(f.data[3] == nullptr);
  }

  SECTION("rgba")
  {
    f.hdr.pix_fmt_name = ilp_movie::PixFmt::kRGBA_P_F32;
    f.buf = std::make_unique<uint8_t[]>(// NOLINT
      ilp_movie::GetBufferSize(f.hdr.pix_fmt_name, f.hdr.width, f.hdr.height).value());
    REQUIRE(ilp_movie::FillArrays(/*out*/ f.data,
      /*out*/ f.linesize,
      f.buf.get(),
      f.hdr.pix_fmt_name,
      f.hdr.width,
      f.hdr.height));

    const auto w = static_cast<std::size_t>(f.hdr.width);
    const auto h = static_cast<std::size_t>(f.hdr.height);
    REQUIRE(f.linesize[0] == static_cast<int>(w * sizeof(float)));
    REQUIRE(f.linesize[1] == static_cast<int>(w * sizeof(float)));
    REQUIRE(f.linesize[2] == static_cast<int>(w * sizeof(float)));
    REQUIRE(f.linesize[3] == static_cast<int>(w * sizeof(float)));

    const auto line0 = static_cast<std::size_t>(f.linesize[0]);
    const auto line1 = static_cast<std::size_t>(f.linesize[1]);
    const auto line2 = static_cast<std::size_t>(f.linesize[2]);
    REQUIRE(f.data[0] == f.buf.get());
    REQUIRE(f.data[1] == f.buf.get() + h * line0);
    REQUIRE(f.data[2] == f.buf.get() + h * (line0 + line1));
    REQUIRE(f.data[3] == f.buf.get() + h * (line0 + line1 + line2));
  }
}

TEST_CASE("CompPixelData")
{
  using ilp_movie::CompPixelData;
  namespace Comp = ilp_movie::Comp;

  constexpr int w = 64;
  constexpr int h = 48;

  ilp_movie::Frame f{};
  f.hdr.width = w;
  f.hdr.height = h;

  SECTION("rgb")
  {
    f.hdr.pix_fmt_name = ilp_movie::PixFmt::kRGB_P_F32;
    f.buf = std::make_unique<uint8_t[]>(// NOLINT
      ilp_movie::GetBufferSize(f.hdr.pix_fmt_name, f.hdr.width, f.hdr.height).value());
    REQUIRE(ilp_movie::FillArrays(/*out*/ f.data,
      /*out*/ f.linesize,
      f.buf.get(),
      f.hdr.pix_fmt_name,
      f.hdr.width,
      f.hdr.height));

    const auto r = CompPixelData<float>(f.data, Comp::kR, w, h, f.hdr.pix_fmt_name);
    const auto g = CompPixelData<float>(f.data, Comp::kG, w, h, f.hdr.pix_fmt_name);
    const auto b = CompPixelData<float>(f.data, Comp::kB, w, h, f.hdr.pix_fmt_name);
    const auto a = CompPixelData<float>(f.data, Comp::kA, w, h, f.hdr.pix_fmt_name);

    REQUIRE(!Empty(r));
    REQUIRE(!Empty(g));
    REQUIRE(!Empty(b));
    REQUIRE(Empty(a));

    REQUIRE(reinterpret_cast<uint8_t*>(g.data) == f.data[0]);
    REQUIRE(reinterpret_cast<uint8_t*>(b.data) == f.data[1]);
    REQUIRE(reinterpret_cast<uint8_t*>(r.data) == f.data[2]);

    const auto line0 = static_cast<std::size_t>(f.linesize[0]);
    const auto line1 = static_cast<std::size_t>(f.linesize[1]);
    const auto line2 = static_cast<std::size_t>(f.linesize[2]);
    REQUIRE(g.count == h * (line0 / sizeof(float)));
    REQUIRE(b.count == h * (line1 / sizeof(float)));
    REQUIRE(r.count == h * (line2 / sizeof(float)));
  }

  SECTION("rgba") {}
}

}// namespace
