#include "CurveModel.h"
#include <algorithm>
#include <cmath>

namespace at
{
static float bend (float t, float tension)
{
    if (std::abs (tension) < 1e-4f) return t;
    if (tension > 0) return std::pow (t, 1.0f + 4.0f * tension);
    return 1.0f - std::pow (1.0f - t, 1.0f - 4.0f * tension);
}

float Lane::shapeSegment (float t, Shape s, float tension)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    switch (s)
    {
        case Shape::Linear: return bend (t, tension);
        case Shape::Hold:   return t >= 1.0f ? 1.0f : 0.0f;
        case Shape::Swoop:
        {
            const float len = juce::jlimit (0.04f, 0.9f, 0.3f + 0.26f * tension);
            const float start = 1.0f - len;
            if (t <= start) return 0.0f;
            const float u = (t - start) / len;
            return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * u);
        }
        case Shape::Smooth:
        default:
        {
            const float u = bend (t, tension);
            return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * u);
        }
    }
}

float Lane::evaluate (double beat, int& hint) const
{
    const int n = (int) nodes.size();
    if (n == 0) return defaultValue;
    if (beat <= nodes[0].beat) { hint = 0; return nodes[0].value; }
    if (beat >= nodes[(size_t) n - 1].beat) { hint = n - 1; return nodes[(size_t) n - 1].value; }

    int i = juce::jlimit (0, n - 2, hint);
    if (! (nodes[(size_t) i].beat <= beat && beat < nodes[(size_t) i + 1].beat))
    {
        // forward step is the common case during playback
        if (nodes[(size_t) i + 1].beat <= beat && i + 2 < n && beat < nodes[(size_t) i + 2].beat)
            ++i;
        else
        {
            auto it = std::upper_bound (nodes.begin(), nodes.end(), beat,
                                        [] (double b, const Node& nd) { return b < nd.beat; });
            i = (int) std::distance (nodes.begin(), it) - 1;
            i = juce::jlimit (0, n - 2, i);
        }
    }
    hint = i;
    const auto& a = nodes[(size_t) i];
    const auto& b = nodes[(size_t) i + 1];
    const double span = b.beat - a.beat;
    if (span <= 1e-9) return b.value;
    const float t = (float) ((beat - a.beat) / span);
    return a.value + (b.value - a.value) * shapeSegment (t, a.shape, a.tension);
}

void Lane::sort()
{
    std::stable_sort (nodes.begin(), nodes.end(), [] (const Node& x, const Node& y) { return x.beat < y.beat; });
}

int Lane::insert (Node nd)
{
    auto it = std::upper_bound (nodes.begin(), nodes.end(), nd.beat,
                                [] (double b, const Node& x) { return b < x.beat; });
    it = nodes.insert (it, nd);
    return (int) std::distance (nodes.begin(), it);
}

void Lane::removeRange (double b0, double b1)
{
    nodes.erase (std::remove_if (nodes.begin(), nodes.end(),
                                 [=] (const Node& x) { return x.beat >= b0 && x.beat <= b1; }),
                 nodes.end());
}

juce::String Lane::toString() const
{
    juce::String s;
    s.preallocateBytes (nodes.size() * 28);
    for (auto& nd : nodes)
        s << juce::String (nd.beat, 5) << "," << juce::String (nd.value, 4) << ","
          << (int) nd.shape << "," << juce::String (nd.tension, 3) << ";";
    return s;
}

void Lane::fromString (const juce::String& str)
{
    nodes.clear();
    juce::StringArray items;
    items.addTokens (str, ";", "");
    for (auto& it : items)
    {
        juce::StringArray f;
        f.addTokens (it, ",", "");
        if (f.size() < 2) continue;
        Node nd;
        nd.beat = f[0].getDoubleValue();
        nd.value = f[1].getFloatValue();
        nd.shape = f.size() > 2 ? (Shape) juce::jlimit (0, 3, f[2].getIntValue()) : Shape::Smooth;
        nd.tension = f.size() > 3 ? juce::jlimit (-1.0f, 1.0f, f[3].getFloatValue()) : 0.0f;
        nodes.push_back (nd);
    }
    sort();
}

std::unique_ptr<juce::XmlElement> Pattern::toXml() const
{
    auto x = std::make_unique<juce::XmlElement> ("Pattern");
    x->setAttribute ("bars", lengthBars);
    x->setAttribute ("bpb", beatsPerBar);
    x->setAttribute ("loop", loop);
    x->setAttribute ("pitch", pitch.toString());
    x->setAttribute ("volume", volume.toString());
    return x;
}

void Pattern::fromXml (const juce::XmlElement& x)
{
    lengthBars = juce::jlimit (1, 64, x.getIntAttribute ("bars", 4));
    beatsPerBar = juce::jlimit (1, 16, x.getIntAttribute ("bpb", 4));
    loop = x.getBoolAttribute ("loop", true);
    pitch.fromString (x.getStringAttribute ("pitch"));
    volume.fromString (x.getStringAttribute ("volume"));
}

void Pattern::copyFrom (const Pattern& o)
{
    pitch.nodes.assign (o.pitch.nodes.begin(), o.pitch.nodes.end());
    volume.nodes.assign (o.volume.nodes.begin(), o.volume.nodes.end());
    pitch.defaultValue = o.pitch.defaultValue;
    volume.defaultValue = o.volume.defaultValue;
    lengthBars = o.lengthBars;
    beatsPerBar = o.beatsPerBar;
    loop = o.loop;
}

static void rdp (const std::vector<std::pair<double, float>>& p, size_t a, size_t b, float eps,
                 std::vector<bool>& keep)
{
    if (b <= a + 1) return;
    const double ba = p[a].first, bb = p[b].first;
    float worst = 0; size_t wi = a;
    for (size_t i = a + 1; i < b; ++i)
    {
        const double t = (bb - ba) > 1e-12 ? (p[i].first - ba) / (bb - ba) : 0.0;
        const float lin = p[a].second + (float) t * (p[b].second - p[a].second);
        const float d = std::abs (p[i].second - lin);
        if (d > worst) { worst = d; wi = i; }
    }
    if (worst > eps)
    {
        keep[wi] = true;
        rdp (p, a, wi, eps, keep);
        rdp (p, wi, b, eps, keep);
    }
}

std::vector<Node> simplifyTrace (const std::vector<std::pair<double, float>>& pts, float epsilon, Shape shape)
{
    std::vector<Node> out;
    if (pts.empty()) return out;
    std::vector<bool> keep (pts.size(), false);
    keep.front() = keep.back() = true;
    rdp (pts, 0, pts.size() - 1, epsilon, keep);
    for (size_t i = 0; i < pts.size(); ++i)
        if (keep[i]) out.push_back ({ pts[i].first, pts[i].second, shape, 0.0f });
    return out;
}
} // namespace at
