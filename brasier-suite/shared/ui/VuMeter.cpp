#include "VuMeter.h"
#include "BrasierLookAndFeel.h"

namespace brasier::ui
{
    namespace
    {
        constexpr float minVu = -20.0f;
        constexpr float maxVu = 3.0f;
        constexpr float sweepRadians = 0.85f;   // +/- ~49 degres
    }

    VuMeter::VuMeter (juce::String text) : caption (std::move (text))
    {
        setOpaque (false);
    }

    float VuMeter::vuToPosition (float vu) noexcept
    {
        // Echelle d'un vrai VU : position proportionnelle a la tension
        const float g = std::pow (10.0f, juce::jlimit (minVu, maxVu, vu) / 20.0f);
        const float gMin = std::pow (10.0f, minVu / 20.0f);
        const float gMax = std::pow (10.0f, maxVu / 20.0f);
        return (g - gMin) / (gMax - gMin);
    }

    void VuMeter::update (float meanSquare, float peak, float deltaSeconds)
    {
        const float dbfs = meanSquare > 1.0e-12f ? 10.0f * std::log10 (meanSquare) : -120.0f;
        const float targetVu = juce::jlimit (minVu - 2.0f, maxVu + 1.0f, dbfs - referenceDbfs);

        // 99 % en 300 ms : constante de temps ~65 ms
        const float coeff = 1.0f - std::exp (-deltaSeconds / 0.065f);
        const float previous = needleVu;
        needleVu += coeff * (targetVu - needleVu);

        if (peak > 0.966f)   // -0,3 dBFS
            clipHold = 1.0f;
        else
            clipHold = juce::jmax (0.0f, clipHold - deltaSeconds);

        if (std::abs (needleVu - previous) > 0.01f || clipHold > 0.0f)
            repaint();
    }

    void VuMeter::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();

        // Boitier
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 6.0f);
        juce::ColourGradient bezel (colours::panelEdge, 0.0f, bounds.getY(), colours::engraving, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (bezel);
        g.fillRoundedRectangle (bounds, 6.0f);

        // Cadran creme
        const auto face = bounds.reduced (5.0f);
        juce::ColourGradient cream (juce::Colour (0xfff3e7cf), face.getCentreX(), face.getY(),
                                    juce::Colour (0xffd8c7a3), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (cream);
        g.fillRoundedRectangle (face, 3.0f);

        g.saveState();
        g.reduceClipRegion (face.toNearestInt());

        const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.32f);
        const float radius = face.getHeight() * 1.02f;
        auto angleFor = [] (float vu) { return -sweepRadians + 2.0f * sweepRadians * vuToPosition (vu); };
        auto pointAt = [&pivot] (float angle, float r) { return juce::Point<float> (pivot.x + r * std::sin (angle), pivot.y - r * std::cos (angle)); };

        // Arc d'echelle (noir) et zone rouge
        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, radius * 0.86f, radius * 0.86f, 0.0f, angleFor (minVu), angleFor (0.0f), true);
        g.setColour (juce::Colour (0xff2a2622));
        g.strokePath (arc, juce::PathStrokeType (1.4f));

        juce::Path red;
        red.addCentredArc (pivot.x, pivot.y, radius * 0.875f, radius * 0.875f, 0.0f, angleFor (0.0f), angleFor (maxVu), true);
        g.setColour (juce::Colour (0xffc0392b));
        g.strokePath (red, juce::PathStrokeType (4.0f));

        // Graduations et chiffres
        const float marks[] = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
        g.setFont (juce::FontOptions (juce::jmax (8.0f, face.getHeight() * 0.11f)));
        for (float m : marks)
        {
            const float a = angleFor (m);
            g.setColour (m > 0.0f ? juce::Colour (0xffc0392b) : juce::Colour (0xff2a2622));
            g.drawLine ({ pointAt (a, radius * 0.86f), pointAt (a, radius * 0.94f) }, 1.2f);

            const auto label = (m > 0.0f ? "+" : "") + juce::String (static_cast<int> (m));
            const auto p = pointAt (a, radius * 1.03f);
            g.drawText (m <= -10.0f || m >= -7.0f ? label : juce::String(),
                        juce::Rectangle<float> (26.0f, 12.0f).withCentre (p), juce::Justification::centred, false);
        }

        g.setColour (juce::Colour (0xff2a2622));
        g.setFont (juce::FontOptions (face.getHeight() * 0.2f, juce::Font::bold));
        g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.42f).withHeight (face.getHeight() * 0.25f),
                    juce::Justification::centred, false);
        g.setFont (juce::FontOptions (face.getHeight() * 0.11f, juce::Font::bold));
        g.setColour (juce::Colour (0xff6b6255));
        g.drawText (caption, face.withTrimmedTop (face.getHeight() * 0.66f).withHeight (face.getHeight() * 0.16f),
                    juce::Justification::centred, false);

        // Aiguille (avec ombre)
        const float a = angleFor (needleVu);
        const auto tip = pointAt (a, radius * 0.97f);
        const auto base = pointAt (a, radius * 0.25f);
        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.drawLine ({ base.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f) }, 1.6f);
        g.setColour (juce::Colour (0xff1a1714));
        g.drawLine ({ base, tip }, 1.5f);

        g.restoreState();

        // Verre : reflet
        juce::ColourGradient glass (juce::Colours::white.withAlpha (0.22f), face.getX(), face.getY(),
                                    juce::Colours::white.withAlpha (0.0f), face.getX(), face.getCentreY(), false);
        g.setGradientFill (glass);
        g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.5f), 3.0f);

        // LED de crete
        const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ face.getRight() - 9.0f, face.getY() + 9.0f });
        if (clipHold > 0.0f)
        {
            g.setColour (juce::Colours::red.withAlpha (0.4f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (clipHold > 0.0f ? juce::Colour (0xffff3b30) : juce::Colour (0xff5a2a24));
        g.fillEllipse (led);
    }
}
