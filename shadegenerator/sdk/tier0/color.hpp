#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>

class Color {
private:
	unsigned char colors[4];

public:
	constexpr Color()
		: colors{ 0, 0, 0, 0 } {}

	constexpr Color(int r, int g, int b, int a = 255)
		: colors{ static_cast<unsigned char>(r),
		          static_cast<unsigned char>(g),
		          static_cast<unsigned char>(b),
		          static_cast<unsigned char>(a) } {}

	constexpr Color(float r, float g, float b, float a = 1.0f)
		: colors{ static_cast<unsigned char>(r * 255.0f),
		          static_cast<unsigned char>(g * 255.0f),
		          static_cast<unsigned char>(b * 255.0f),
		          static_cast<unsigned char>(a * 255.0f) } {}

	constexpr explicit Color(unsigned long hex)
		: colors{ static_cast<unsigned char>((hex >> 24) & 0xFF),
		          static_cast<unsigned char>((hex >> 16) & 0xFF),
		          static_cast<unsigned char>((hex >> 8)  & 0xFF),
		          static_cast<unsigned char>((hex >> 0)  & 0xFF) } {}

	[[nodiscard]] constexpr unsigned long ByteColorRGBA() const {
		return	(static_cast<unsigned long>(colors[0]) << (3 * 8)) |
				(static_cast<unsigned long>(colors[1]) << (2 * 8)) |
				(static_cast<unsigned long>(colors[2]) << (1 * 8)) |
				(static_cast<unsigned long>(colors[3]) << (0 * 8));
	}

	[[nodiscard]] inline constexpr int r() const { return colors[0]; }
	[[nodiscard]] inline constexpr int g() const { return colors[1]; }
	[[nodiscard]] inline constexpr int b() const { return colors[2]; }
	[[nodiscard]] inline constexpr int a() const { return colors[3]; }

	void SetRawColor(uint32_t clr) {
		std::memcpy(colors, &clr, sizeof(clr));
	}

	[[nodiscard]] uint32_t GetRawColor() const {
		uint32_t result;
		std::memcpy(&result, colors, sizeof(result));
		return result;
	}


    void GetHSV(unsigned char& h, unsigned char& s, unsigned char& v) const {
        float r = colors[0] / 255.0f;
        float g = colors[1] / 255.0f;
        float b = colors[2] / 255.0f;

        float cmax = r > g ? (r > b ? r : b) : (g > b ? g : b);
        float cmin = r < g ? (r < b ? r : b) : (g < b ? g : b);
        float delta = cmax - cmin;

        float hf = 0.0f;
        if (delta > 0.0f) {
            if (cmax == r)      hf = 60.0f * (fmodf((g - b) / delta, 6.0f));
            else if (cmax == g) hf = 60.0f * ((b - r) / delta + 2.0f);
            else                hf = 60.0f * ((r - g) / delta + 4.0f);
            if (hf < 0.0f) hf += 360.0f;
        }

        float sf = cmax > 0.0f ? delta / cmax : 0.0f;
        float vf = cmax;

        h = static_cast<unsigned char>(hf / 360.0f * 255.0f);
        s = static_cast<unsigned char>(sf * 255.0f);
        v = static_cast<unsigned char>(vf * 255.0f);
    }


    static Color FromHSV(unsigned char h, unsigned char s, unsigned char v, unsigned char a = 255) {
        Color c; c.SetColorFromHSV(h, s, v, a); return c;
    }


    void SetColorFromHSVf(float hf360, float sf, float vf, float af = 1.0f) {
        float hf = hf360 * 360.0f;
        float c  = vf * sf;
        float x  = c * (1.0f - fabsf(fmodf(hf / 60.0f, 2.0f) - 1.0f));
        float m  = vf - c;

        float r, g, b;
        int   sector = static_cast<int>(hf / 60.0f) % 6;
        switch (sector) {
            case 0: r = c; g = x; b = 0; break;
            case 1: r = x; g = c; b = 0; break;
            case 2: r = 0; g = c; b = x; break;
            case 3: r = 0; g = x; b = c; break;
            case 4: r = x; g = 0; b = c; break;
            default:r = c; g = 0; b = x; break;
        }

        colors[0] = static_cast<unsigned char>((r + m) * 255.0f + 0.5f);
        colors[1] = static_cast<unsigned char>((g + m) * 255.0f + 0.5f);
        colors[2] = static_cast<unsigned char>((b + m) * 255.0f + 0.5f);
        colors[3] = static_cast<unsigned char>(af * 255.0f + 0.5f);
    }

    void SetColorFromHSV(unsigned char h, unsigned char s, unsigned char v, unsigned char a = 255) {
        float hf = h / 255.0f * 360.0f;
        float sf = s / 255.0f;
        float vf = v / 255.0f;

        float c  = vf * sf;
        float x  = c * (1.0f - fabsf(fmodf(hf / 60.0f, 2.0f) - 1.0f));
        float m  = vf - c;

        float r, g, b;
        int   sector = static_cast<int>(hf / 60.0f) % 6;
        switch (sector) {
            case 0: r = c; g = x; b = 0; break;
            case 1: r = x; g = c; b = 0; break;
            case 2: r = 0; g = c; b = x; break;
            case 3: r = 0; g = x; b = c; break;
            case 4: r = x; g = 0; b = c; break;
            default:r = c; g = 0; b = x; break;
        }

        colors[0] = static_cast<unsigned char>((r + m) * 255.0f);
        colors[1] = static_cast<unsigned char>((g + m) * 255.0f);
        colors[2] = static_cast<unsigned char>((b + m) * 255.0f);
        colors[3] = static_cast<unsigned char>(a);
    }

	void SetColor(int r, int g, int b, int a = 255);
	void SetColor(float r, float g, float b, float a = 1.0f);
    void SetColor(const Color& color);
    void SetColor(unsigned long hex);
	void GetColor(int& r, int& g, int& b, int& a) const;

	[[nodiscard]] constexpr unsigned char& operator[](size_t index) {
		return colors[index];
	}

	[[nodiscard]] constexpr const unsigned char& operator[](size_t index) const {
		return colors[index];
	}

    [[nodiscard]] bool IsEmpty() const {
        return colors[0] == 0 && colors[1] == 0 && colors[2] == 0 && colors[3] == 0;
    }

	bool operator==(const Color& clr) const;
	bool operator!=(const Color& clr) const;
	Color& operator=(const Color& clr);
};
