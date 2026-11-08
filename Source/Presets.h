#pragma once
// Factory presets: sound settings plus an original example pattern for the Auto editor.
#include <functional>
#include <vector>
#include "model/CurveModel.h"

namespace at
{
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<juce::String, float>> params;
    std::function<void (Pattern&)> build;
};

namespace presetdetail
{
    struct N { double b; float v; Shape s = Shape::Swoop; float t = 0; };
    inline void fill (Lane& lane, std::initializer_list<N> nodes)
    {
        for (auto& n : nodes) lane.nodes.push_back ({ n.b, n.v, n.s, n.t });
        lane.sort();
    }
    constexpr Shape Sm = Shape::Smooth, Li = Shape::Linear, Ho = Shape::Hold, Sw = Shape::Swoop;
}

inline const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace presetdetail;
    static const std::vector<FactoryPreset> list {
        { "Blank Lab", {}, [] (Pattern& p) { p.lengthBars = 4; } },

        { "Valve Lullaby",
          { { "glide", 45 }, { "timbre", 0.3f }, { "vibDepth", 20 }, { "vibRate", 5.4f },
            { "verbMix", 0.34f }, { "verbSize", 0.65f }, { "echoMix", 0.2f }, { "correction", 0.25f },
            { "scale", 2 }, { "key", 9 } },
          [] (Pattern& p)
          {
              p.lengthBars = 8;
              fill (p.pitch, { { 0, 69 }, { 3, 72 }, { 4, 76 }, { 7.5, 74 }, { 8, 72 }, { 10, 71 }, { 12, 69, Ho },
                               { 16, 64 }, { 18, 69 }, { 20, 72 }, { 22, 71 }, { 24, 67 }, { 26, 64 }, { 28, 69, Ho }, { 31.9, 69 } });
              fill (p.volume, { { 0, 0, Sm }, { 0.2, 0.85f, Li }, { 7.6, 0.85f, Li }, { 7.85, 0.35f, Li }, { 8.1, 0.85f, Li },
                                { 15.2, 0.8f, Sm }, { 15.9, 0, Li }, { 16.05, 0, Sm }, { 16.35, 0.85f, Li }, { 23.7, 0.85f, Li },
                                { 23.95, 0.45f, Li }, { 24.25, 0.85f, Li }, { 31.0, 0.7f, Sm }, { 31.9, 0, Li } });
          } },

        { "Saucer Descent",
          { { "glide", 70 }, { "timbre", 0.25f }, { "vibDepth", 26 }, { "verbMix", 0.45f }, { "verbSize", 0.85f },
            { "echoMix", 0.3f }, { "echoFeedback", 0.46f }, { "output", -2.5f } },
          [] (Pattern& p)
          {
              p.lengthBars = 4;
              fill (p.pitch, { { 0, 91, Sm, 0.4f }, { 4, 60, Ho }, { 6, 63, Sm }, { 8, 55, Li }, { 11, 67, Sm },
                               { 12, 79, Sm, -0.3f }, { 14, 48, Ho }, { 15.9, 48 } });
              fill (p.volume, { { 0, 0, Sm }, { 0.6, 0.9f, Li }, { 13.5, 0.9f, Sm }, { 15.8, 0, Li } });
          } },

        { "Ether Wail",
          { { "glide", 35 }, { "timbre", 0.55f }, { "warmth", 0.45f }, { "brightness", 0.2f }, { "vibDepth", 24 },
            { "vibRate", 6.2f }, { "verbMix", 0.3f } },
          [] (Pattern& p)
          {
              p.lengthBars = 4;
              fill (p.pitch, { { 0, 64 }, { 1, 76 }, { 3, 74 }, { 4, 71 }, { 6, 79 }, { 7.5, 78 }, { 8, 76, Sm },
                               { 9, 88 }, { 11, 86 }, { 12, 83 }, { 14, 76, Ho }, { 15.9, 76 } });
              fill (p.volume, { { 0, 0, Sm }, { 0.25, 0.9f, Li }, { 15.3, 0.9f, Sm }, { 15.9, 0, Li } });
          } },

        { "Martian Radio",
          { { "glide", 12 }, { "correction", 0.85f }, { "scale", 7 }, { "key", 0 }, { "vibDepth", 6 },
            { "timbre", 0.6f }, { "echoDiv", 3 }, { "echoMix", 0.35f }, { "echoFeedback", 0.45f }, { "verbMix", 0.22f } },
          [] (Pattern& p)
          {
              p.lengthBars = 2;
              const float seq[] = { 72, 67, 70, 63, 65, 60, 67, 72, 75, 70, 67, 65, 63, 67, 70, 60,
                                    79, 75, 72, 70, 67, 70, 72, 75, 77, 72, 70, 67, 65, 63, 60, 60 };
              for (int i = 0; i < 32; ++i) p.pitch.nodes.push_back ({ i * 0.25, seq[i], Shape::Linear, 0 });
              for (int i = 0; i < 16; ++i)
              {
                  p.volume.nodes.push_back ({ i * 0.5, 0.25f, Shape::Linear, 0 });
                  p.volume.nodes.push_back ({ i * 0.5 + 0.04, 0.85f, Shape::Linear, 0.4f });
              }
              p.volume.nodes.push_back ({ 7.95, 0.25f, Shape::Linear, 0 });
          } },

        { "Haunted Corridor",
          { { "glide", 90 }, { "timbre", 0.18f }, { "brightness", -0.5f }, { "vibDepth", 28 }, { "vibRate", 4.8f },
            { "verbMix", 0.55f }, { "verbSize", 0.92f }, { "verbDamp", 0.6f }, { "echoMix", 0.18f }, { "output", 3.0f } },
          [] (Pattern& p)
          {
              p.lengthBars = 8;
              fill (p.pitch, { { 0, 48, Sm }, { 6, 51, Sm }, { 10, 47, Sm }, { 16, 55, Sm, 0.5f }, { 22, 53, Sm },
                               { 26, 50, Sm }, { 31.9, 48, Sm } });
              fill (p.volume, { { 0, 0, Sm }, { 3, 0.7f, Sm }, { 8, 0.35f, Sm }, { 12, 0.8f, Sm }, { 18, 0.25f, Sm },
                                { 22, 0.75f, Sm }, { 28, 0.6f, Sm }, { 31.9, 0, Sm } });
          } },

        { "Lab Alarm",
          { { "glide", 5 }, { "vibDepth", 0 }, { "timbre", 0.72f }, { "warmth", 0.6f }, { "echoMix", 0.15f },
            { "verbMix", 0.18f }, { "output", 1.5f } },
          [] (Pattern& p)
          {
              p.lengthBars = 1;
              fill (p.pitch, { { 0, 72, Li }, { 1, 84, Li }, { 2, 72, Li }, { 3, 84, Li }, { 3.99, 72, Li } });
              fill (p.volume, { { 0, 0.8f, Li } });
          } },

        { "Moonbase Serenade",
          { { "glide", 40 }, { "correction", 0.35f }, { "scale", 4 }, { "key", 2 }, { "timbre", 0.38f },
            { "formant", 0.5f }, { "vibDepth", 20 }, { "verbMix", 0.36f }, { "echoMix", 0.24f } },
          [] (Pattern& p)
          {
              p.lengthBars = 8;
              fill (p.pitch, { { 0, 62 }, { 2, 69 }, { 3, 67 }, { 4, 65 }, { 5, 64 }, { 6, 62, Ho }, { 8, 57 },
                               { 10, 62 }, { 12, 65 }, { 13, 67 }, { 14, 69 }, { 15.5, 71 }, { 16, 72, Ho }, { 19, 71 },
                               { 20, 69 }, { 22, 65 }, { 24, 67 }, { 26, 64 }, { 28, 62, Ho }, { 31.9, 62 } });
              fill (p.volume, { { 0, 0, Sm }, { 0.2, 0.85f, Li }, { 7.7, 0.8f, Li }, { 7.95, 0.1f, Li }, { 8.15, 0.85f, Li },
                                { 15.8, 0.85f, Li }, { 16.0, 0.6f, Li }, { 16.2, 0.9f, Li }, { 27.7, 0.8f, Li },
                                { 27.95, 0.2f, Li }, { 28.1, 0.85f, Li }, { 31.5, 0.6f, Sm }, { 31.95, 0, Li } });
          } },
    };
    return list;
}
} // namespace at
