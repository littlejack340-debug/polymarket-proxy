#pragma once

#include <array>
#include <memory>

#include <juce_dsp/juce_dsp.h>

namespace brasier::dsp
{
    /**
        Oversampling 1x / 2x / 4x / 8x commutable en temps reel.

        Pourquoi : une non-linearite (saturation, clip...) cree des harmoniques
        au-dessus de Nyquist qui se "replient" (aliasing) en frequences
        inharmoniques audibles. On travaille donc a N fois la frequence
        d'echantillonnage, puis on filtre avant de redescendre.

        Implementation : filtres demi-bande FIR equiripple de JUCE (phase
        lineaire, donc compatibles avec un melange dry/wet sans "phasing"), en
        cascade (8x = 3 etages 2x), avec latence entiere pour pouvoir la reporter
        exactement a l'hote.

        Les trois moteurs (2x, 4x, 8x) sont tous alloues dans prepare() : changer
        de facteur pendant la lecture ne fait donc aucune allocation, on bascule
        simplement de moteur (et on remet son etat a zero).
    */
    class Oversampler
    {
    public:
        static constexpr int numFactors = 4;   // index 0..3 -> 1x, 2x, 4x, 8x
        static constexpr int maxFactor  = 8;

        /** Alloue les trois moteurs (hors thread audio). */
        void prepare (int numChannels, int maxBlockSize);
        void reset() noexcept;

        /** Selectionne le facteur (0 = 1x, 1 = 2x, 2 = 4x, 3 = 8x). Temps reel. */
        void setFactorIndex (int newIndex) noexcept;
        int getFactorIndex() const noexcept { return index; }
        int getFactor() const noexcept      { return 1 << index; }

        /** Latence (en echantillons a la frequence de base) du facteur courant. */
        int getLatencySamples() const noexcept;
        int getLatencySamplesForIndex (int factorIndex) const noexcept;
        int getMaxLatencySamples() const noexcept;

        /** Sur-echantillonne le bloc. En 1x, renvoie le bloc lui-meme. */
        juce::dsp::AudioBlock<float> processUp (juce::dsp::AudioBlock<float> block) noexcept;

        /** Redescend dans le bloc de sortie (taille de base). En 1x, ne fait rien. */
        void processDown (juce::dsp::AudioBlock<float> block) noexcept;

    private:
        juce::dsp::Oversampling<float>* current() const noexcept
        {
            return index > 0 ? engines[static_cast<size_t> (index - 1)].get() : nullptr;
        }

        std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, numFactors - 1> engines;
        int index = 0;
    };
}
