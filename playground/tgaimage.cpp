#include <iostream>
#include <cstring>
#include "tgaimage.h"

struct TGAImageMappedWriter {
    HANDLE file  = nullptr;
    HANDLE map   = nullptr;
    uint8_t* ptr = nullptr;
    size_t size  = 0;
};

// --------------------------------------------------------------
// Create a persistent memory-mapped file for writing TGAs fast
// --------------------------------------------------------------
bool tga_mmap_init(TGAImageMappedWriter &mw, const char* filename, size_t size_bytes)
{
    mw.size = size_bytes;

    mw.file = CreateFileA(
        filename,
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (mw.file == INVALID_HANDLE_VALUE) return false;

    mw.map = CreateFileMappingA(
        mw.file,
        nullptr,
        PAGE_READWRITE,
        0,
        (DWORD)size_bytes,
        nullptr
    );
    if (!mw.map) return false;

    mw.ptr = (uint8_t*)MapViewOfFile(
        mw.map,
        FILE_MAP_WRITE | FILE_MAP_READ,
        0, 0,
        size_bytes
    );
    if (!mw.ptr) return false;

    // Pre-touch pages to avoid page faults later
    for (size_t i = 0; i < size_bytes; i += 4096) mw.ptr[i] = mw.ptr[i];

    return true;
}

// --------------------------------------------------------------
void tga_mmap_close(TGAImageMappedWriter &mw)
{
    if (mw.ptr) UnmapViewOfFile(mw.ptr);
    if (mw.map) CloseHandle(mw.map);
    if (mw.file) CloseHandle(mw.file);
}

// --------------------------------------------------------------
// Write a TGA using the mapped writer
// --------------------------------------------------------------
void tga_mmap_write(
    TGAImageMappedWriter &mw,
    const TGAImage& img,
    bool vflip = true,
    bool rle   = false     // mmap version does not yet support RLE
)
{
    uint8_t* out = mw.ptr;

    // Create a matching header based on your struct
    TGAHeader header = {};
    header.width         = img.width();
    header.height        = img.height();
    header.bitsperpixel  = img.bpp * 8;
    header.datatypecode  = (img.bpp == TGAImage::GRAYSCALE ? 3 : 2);
    header.imagedescriptor = vflip ? 0x00 : 0x20;

    // Write header (fixed 18 bytes)
    memcpy(out, &header, sizeof(header));
    out += sizeof(header);

    // Write raw pixel data (w * h * bpp)
    size_t pixel_bytes = size_t(img.width()) * img.height() * img.bpp;
    memcpy(out, img.data.data(), pixel_bytes);
    out += pixel_bytes;

    // Write footer
    static const uint8_t footer[26] = "TRUEVISION-XFILE.\0";
    memcpy(out, footer, sizeof(footer));
}

TGAImage::TGAImage(const int w, const int h, const int bpp) : w(w), h(h), bpp(bpp), data(w*h*bpp, 0) {}

bool TGAImage::write_tga_file_mmap(const std::string& filename, bool vflip, bool rle) const
{
    aim_profiler_time_function;
    // RLE unsupported in mmap version
    if (rle) return write_tga_file(filename, vflip, rle);

    size_t total_size =
        sizeof(TGAHeader)
        + (size_t)w * h * bpp
        + 26;  // footer

    TGAImageMappedWriter mw;
    if (!tga_mmap_init(mw, filename.c_str(), total_size)) {
        return false;
    }

    tga_mmap_write(mw, *this, vflip, false);
    tga_mmap_close(mw);

    return true;
}

bool TGAImage::read_tga_file(const std::string filename) {
    std::ifstream in;
    in.open(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "can't open file " << filename << "\n";
        return false;
    }
    TGAHeader header;
    in.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!in.good()) {
        std::cerr << "an error occured while reading the header\n";
        return false;
    }
    w   = header.width;
    h   = header.height;
    bpp = header.bitsperpixel>>3;
    if (w<=0 || h<=0 || (bpp!=GRAYSCALE && bpp!=RGB && bpp!=RGBA)) {
        std::cerr << "bad bpp (or width/height) value\n";
        return false;
    }
    size_t nbytes = bpp*w*h;
    data = std::vector<std::uint8_t>(nbytes, 0);
    if (3==header.datatypecode || 2==header.datatypecode) {
        in.read(reinterpret_cast<char *>(data.data()), nbytes);
        if (!in.good()) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
    } else if (10==header.datatypecode||11==header.datatypecode) {
        if (!load_rle_data(in)) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
    } else {
        std::cerr << "unknown file format " << (int)header.datatypecode << "\n";
        return false;
    }
    if (!(header.imagedescriptor & 0x20))
        flip_vertically();
    if (header.imagedescriptor & 0x10)
        flip_horizontally();
    std::cerr << w << "x" << h << "/" << bpp*8 << "\n";
    return true;
}

bool TGAImage::load_rle_data(std::ifstream &in) {
    size_t pixelcount = w*h;
    size_t currentpixel = 0;
    size_t currentbyte  = 0;
    TGAColor colorbuffer;
    do {
        std::uint8_t chunkheader = 0;
        chunkheader = in.get();
        if (!in.good()) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
        if (chunkheader<128) {
            chunkheader++;
            for (int i=0; i<chunkheader; i++) {
                in.read(reinterpret_cast<char *>(colorbuffer.bgra), bpp);
                if (!in.good()) {
                    std::cerr << "an error occured while reading the header\n";
                    return false;
                }
                for (int t=0; t<bpp; t++)
                    data[currentbyte++] = colorbuffer.bgra[t];
                currentpixel++;
                if (currentpixel>pixelcount) {
                    std::cerr << "Too many pixels read\n";
                    return false;
                }
            }
        } else {
            chunkheader -= 127;
            in.read(reinterpret_cast<char *>(colorbuffer.bgra), bpp);
            if (!in.good()) {
                std::cerr << "an error occured while reading the header\n";
                return false;
            }
            for (int i=0; i<chunkheader; i++) {
                for (int t=0; t<bpp; t++)
                    data[currentbyte++] = colorbuffer.bgra[t];
                currentpixel++;
                if (currentpixel>pixelcount) {
                    std::cerr << "Too many pixels read\n";
                    return false;
                }
            }
        }
    } while (currentpixel < pixelcount);
    return true;
}

bool TGAImage::write_tga_file(const std::string filename, const bool vflip, const bool rle) const {
    aim_profiler_time_function;
    constexpr std::uint8_t developer_area_ref[4] = {0, 0, 0, 0};
    constexpr std::uint8_t extension_area_ref[4] = {0, 0, 0, 0};
    constexpr std::uint8_t footer[18] = {'T','R','U','E','V','I','S','I','O','N','-','X','F','I','L','E','.','\0'};
    std::ofstream out;
    out.open(filename, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "can't open file " << filename << "\n";
        return false;
    }
    TGAHeader header = {};
    header.bitsperpixel = bpp<<3;
    header.width  = w;
    header.height = h;
    header.datatypecode = (bpp==GRAYSCALE ? (rle?11:3) : (rle?10:2));
    header.imagedescriptor = vflip ? 0x00 : 0x20; // top-left or bottom-left origin
    out.write(reinterpret_cast<const char *>(&header), sizeof(header));
    if (!out.good()) goto err;
    if (!rle) {
        out.write(reinterpret_cast<const char *>(data.data()), w*h*bpp);
        if (!out.good()) goto err;
    } else if (!unload_rle_data(out)) goto err;
    out.write(reinterpret_cast<const char *>(developer_area_ref), sizeof(developer_area_ref));
    if (!out.good()) goto err;
    out.write(reinterpret_cast<const char *>(extension_area_ref), sizeof(extension_area_ref));
    if (!out.good()) goto err;
    out.write(reinterpret_cast<const char *>(footer), sizeof(footer));
    if (!out.good()) goto err;
    return true;
err:
    std::cerr << "can't dump the tga file\n";
    return false;
}

bool TGAImage::unload_rle_data(std::ofstream &out) const {
    const std::uint8_t max_chunk_length = 128;
    size_t npixels = w*h;
    size_t curpix = 0;
    while (curpix<npixels) {
        size_t chunkstart = curpix*bpp;
        size_t curbyte = curpix*bpp;
        std::uint8_t run_length = 1;
        bool raw = true;
        while (curpix+run_length<npixels && run_length<max_chunk_length) {
            bool succ_eq = true;
            for (int t=0; succ_eq && t<bpp; t++)
                succ_eq = (data[curbyte+t]==data[curbyte+t+bpp]);
            curbyte += bpp;
            if (1==run_length)
                raw = !succ_eq;
            if (raw && succ_eq) {
                run_length--;
                break;
            }
            if (!raw && !succ_eq)
                break;
            run_length++;
        }
        curpix += run_length;
        out.put(raw ? run_length-1 : run_length+127);
        if (!out.good()) return false;
        out.write(reinterpret_cast<const char *>(data.data()+chunkstart), (raw?run_length*bpp:bpp));
        if (!out.good()) return false;
    }
    return true;
}

TGAColor TGAImage::get(const int x, const int y) const {
    if (!data.size() || x<0 || y<0 || x>=w || y>=h) return {};
    TGAColor ret = {0, 0, 0, 0, bpp};
    const std::uint8_t *p = data.data()+(x+y*w)*bpp;
    for (int i=bpp; i--; ret.bgra[i] = p[i]);
    return ret;
}

void TGAImage::set(int x, int y, const TGAColor &c) {
    if (!data.size() || x<0 || y<0 || x>=w || y>=h) return;
    memcpy(data.data()+(x+y*w)*bpp, c.bgra, bpp);
}


void TGAImage::line(int ax, int ay, int bx, int by, const TGAColor &c)
{
    #if 0
    for (int x=ax; x<=bx; x++) {
        float t = (x-ax) / static_cast<float>(bx-ax);
        int y = std::round( ay + (by-ay)*t );
        this->set(x, y, c);
    }
    return;
    #endif
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep)
    {
        std::swap(ax, ay);
        std::swap(bx, by);

    }
    if (ax>bx) { // make it left−to−right
        std::swap(ax, bx);
        std::swap(ay, by);
    }

    float dx = bx - ax;
    float dy = by - ay;

    float dx_n = dx / sqrt(dx * dx + dy * dy);
    float dy_n = dy / sqrt(dx * dx + dy * dy);

    float t = 0;
    int X = ax;
    while(X <= bx)
    {
        t = (X - ax) / (float)(bx - ax);
        int y = dy * t + ay;
        if (steep)
        {
            this->set(y, X, c);
        }
        else
        {
            this->set(X, y, c);
        }
        t+=1.0;
        if (X == bx) break;
        X += 1;
    }
}

void TGAImage::flip_horizontally() {
    for (int i=0; i<w/2; i++)
        for (int j=0; j<h; j++)
            for (int b=0; b<bpp; b++)
                std::swap(data[(i+j*w)*bpp+b], data[(w-1-i+j*w)*bpp+b]);
}

void TGAImage::flip_vertically() {
    for (int i=0; i<w; i++)
        for (int j=0; j<h/2; j++)
            for (int b=0; b<bpp; b++)
                std::swap(data[(i+j*w)*bpp+b], data[(i+(h-1-j)*w)*bpp+b]);
}

int TGAImage::width() const {
    return w;
}

int TGAImage::height() const {
    return h;
}

