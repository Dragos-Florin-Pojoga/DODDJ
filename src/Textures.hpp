#pragma once

#include <SDL3/SDL.h>


#include <unordered_map>
#include <string>
#include <cmath>
#include <algorithm>

#include "./Logging.hpp"


enum class SpriteType {
    Image,
    Square,
    Circle
};

class TextureCache {
public:
    static SDL_Texture* getShape(SDL_Renderer* renderer, SpriteType type) {
        auto it = cache.find(type);
        if (it != cache.end()) return it->second;

        SDL_Texture* tex = nullptr;
        switch (type) {
            case SpriteType::Square:
                tex = makeWhiteTexture(renderer);
                break;
            case SpriteType::Circle:
                tex = makeCircleSDF(renderer, 128);
                break;
            default:
                return nullptr;
        }
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        cache[type] = tex;
        return tex;
    }

    static void clear(SDL_Renderer* renderer) {
        for (auto& [t, tex] : cache) SDL_DestroyTexture(tex);
        cache.clear();
    }

private:
    static inline std::unordered_map<SpriteType, SDL_Texture*> cache;

    static SDL_Texture* makeWhiteTexture(SDL_Renderer* r) {
        const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32);
        if (!format_details) {
            Logging::log_error("Failed to get pixel format details: ", SDL_GetError());
            return nullptr;
        }

        SDL_Surface* s = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
        if (!s) {
            Logging::log_error("Failed to create surface: ", SDL_GetError());
            return nullptr;
        }

        const Uint32 white_pixel = SDL_MapRGBA(format_details, nullptr, 255, 255, 255, 255);
        
        SDL_FillSurfaceRect(s, nullptr, white_pixel);
        
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, s);
        SDL_DestroySurface(s);
        
        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        } else {
            Logging::log_error("Failed to create texture from surface: ", SDL_GetError());
        }
        
        return tex;
    }

    static SDL_Texture* makeCircleSDF(SDL_Renderer* r, int size) {
        SDL_Surface* surf = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA32);
        if (!surf) {
            Logging::log_error("Failed to create surface: ", SDL_GetError());
            return nullptr;
        }

        const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(surf->format);
        if (!format_details) {
            Logging::log_error("Failed to get pixel format details: ", SDL_GetError());
            SDL_DestroySurface(surf);
            return nullptr;
        }

        Uint32* pixels = (Uint32*)surf->pixels;
        float radius = size * 0.5f - 1;
        
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float dx = x - size * 0.5f;
                float dy = y - size * 0.5f;
                
                float dist = std::sqrt(dx*dx + dy*dy);
                float edge = radius;
                float d = dist - edge;
                
                // SDF for antialiasing
                // smoothing across 4-pixel distance (0.5f / (d * 0.5f)).
                float alpha = std::clamp(0.5f - d * 0.5f, 0.0f, 1.0f);
                Uint8 a = static_cast<Uint8>(alpha * 255);

                pixels[y * size + x] = SDL_MapRGBA(format_details, nullptr, 255, 255, 255, a); 
            }
        }
        
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        
        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
        
        SDL_DestroySurface(surf);
        return tex;
    }
};
