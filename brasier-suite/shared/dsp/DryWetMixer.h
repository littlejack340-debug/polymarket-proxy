#pragma once

#include <juce_dsp/juce_dsp.h>

#include "LinearSmoother.h"

namespace brasier::dsp
{
    /**
        Melangeur dry/wet avec compensation de latence.

        Le signal sec (dry) est retarde d'exactement la latence du chemin traite
        (oversampling, pitch shifter...), sinon le melange creerait un filtrage en
        peigne (effet "phasing") des que Mix < 100 %.

        - Loi lineaire : dry*(1-m) + wet*m. Ideale pour les signaux correles
          (saturation, filtre) : a 50 % le niveau ne bouge pas.
        - Loi a puissance constante : dry*cos + wet*sin. Ideale pour les signaux
          decorreles (delay, reverb) : pas de creux de volume a 50 %.

        Usage par bloc (thread audio, sans allocation) :
            pushDry (bloc d'entree)   -> memorise et aligne le dry
            ... traitement du wet ...
            mixWet (bloc wet)         -> remplace le bloc par le melange
    */
    class DryWetMixer
    {
    public:
        enum class MixLaw { linear, equalPower };

        /** Alloue les buffers (a appeler hors thread audio). */
        void prepare (double sampleRate, int numChannels, int maxBlockSize, int maxLatencySamples);
        void reset() noexcept;

        void setMixLaw (MixLaw newLaw) noexcept      { law = newLaw; }

        /** Latence du chemin wet en echantillons (bornee a maxLatencySamples). Temps reel. */
        void setLatency (int samples) noexcept;
        int getLatency() const noexcept               { return latency; }

        /** Mix cible entre 0 (100 % dry) et 1 (100 % wet), lisse sur ~30 ms. */
        void setMix (float mix01) noexcept            { mixSmoother.setTarget (juce::jlimit (0.0f, 1.0f, mix01)); }
        void setMixImmediate (float mix01) noexcept   { mixSmoother.setCurrentAndTarget (juce::jlimit (0.0f, 1.0f, mix01)); }

        /** Copie l'entree dans la ligne de retard et prepare le dry aligne du bloc. */
        void pushDry (const juce::dsp::AudioBlock<const float>& input) noexcept;

        /** Dry aligne du dernier bloc pousse (meme taille que ce bloc). */
        juce::dsp::AudioBlock<const float> getAlignedDry() const noexcept;

        /** wet <- dry * gainDry + wet * gainWet (wet doit avoir la taille du dernier pushDry). */
        void mixWet (juce::dsp::AudioBlock<float>& wet) noexcept;

    private:
        void computeGains (float mix, float& dryGain, float& wetGain) const noexcept;

        juce::AudioBuffer<float> delayBuffer;   // ligne de retard circulaire par canal
        juce::AudioBuffer<float> alignedDry;    // dry retarde du bloc courant
        LinearSmoother<float> mixSmoother;
        MixLaw law = MixLaw::linear;

        int delaySize = 1;
        int writePos = 0;
        int latency = 0;
        int maxLatency = 0;
        int lastBlockSize = 0;
        int numChans = 0;
    };
}
