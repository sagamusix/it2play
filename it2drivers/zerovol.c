/*
** 8bb:
** ---- Code for "zero-vol" update routines ----
**
** These are used when the final volume is zero, and they'll only update
** the sampling position instead of doing actual mixing. They are the same
** for SB16/"SB16 MMX"/"WAV writer".
*/

#include <stdint.h>
#include "../it_structs.h"
#include "../it_music.h"

void UpdateNoLoop(slaveChn_t *sc, uint32_t numSamples)
{
	const uint64_t SamplesToMix = (uint64_t)sc->Delta * (uint32_t)numSamples;

	uint32_t SampleOffset = sc->SamplingPosition + (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
	sc->SmpError += SamplesToMix & MIX_FRAC_MASK;
	SampleOffset += (uint32_t)sc->SmpError >> MIX_FRAC_BITS;
	sc->SmpError &= MIX_FRAC_MASK;

	if (SampleOffset >= (uint32_t)sc->LoopEnd)
	{
		sc->Flags = SF_NOTE_STOP;
		if (!(sc->HCN & CHN_DISOWNED))
		{
			((hostChn_t *)sc->HCOffst)->Flags &= ~HF_CHAN_ON; // Signify channel off
			return;
		}
	}

	sc->SamplingPosition = SampleOffset;
}

void UpdateForwardsLoop(slaveChn_t *sc, uint32_t numSamples)
{
	const uint64_t SamplesToMix = (uint64_t)sc->Delta * (uint32_t)numSamples;

	sc->SmpError += SamplesToMix & MIX_FRAC_MASK;
	sc->SamplingPosition += sc->SmpError >> MIX_FRAC_BITS;
	sc->SamplingPosition += (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
	sc->SmpError &= MIX_FRAC_MASK;

	if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd) // Reset position...
	{
		const uint32_t LoopLength = sc->LoopEnd - sc->LoopBeg;
		if (LoopLength == 0)
			sc->SamplingPosition = 0;
		else
			sc->SamplingPosition = sc->LoopBeg + ((sc->SamplingPosition - sc->LoopEnd) % LoopLength);
	}
}

void UpdatePingPongLoop(slaveChn_t *sc, uint32_t numSamples)
{
	const uint32_t LoopLength = sc->LoopEnd - sc->LoopBeg;

	const uint64_t SamplesToMix = (uint64_t)sc->Delta * (uint32_t)numSamples;
	uint32_t IntSamples = (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
	uint16_t FracSamples = (uint16_t)(SamplesToMix & MIX_FRAC_MASK);

	if (sc->LpD == DIR_BACKWARDS)
	{
		sc->SmpError -= FracSamples;
		sc->SamplingPosition += ((int32_t)sc->SmpError >> MIX_FRAC_BITS);
		sc->SamplingPosition -= IntSamples;
		sc->SmpError &= MIX_FRAC_MASK;

		if (sc->SamplingPosition <= sc->LoopBeg)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->LoopBeg - sc->SamplingPosition) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = (sc->LoopEnd - 1) + (LoopLength - NewLoopPos);
			}
			else
			{
				sc->LpD = DIR_FORWARDS;
				sc->SamplingPosition = sc->LoopBeg + NewLoopPos;
				sc->SmpError = (uint16_t)(0 - sc->SmpError);
			}
		}
	}
	else // 8bb: forwards
	{
		sc->SmpError += FracSamples;
		sc->SamplingPosition += sc->SmpError >> MIX_FRAC_BITS;
		sc->SamplingPosition += IntSamples;
		sc->SmpError &= MIX_FRAC_MASK;

		if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = sc->LoopBeg + (NewLoopPos - LoopLength);
			}
			else
			{
				sc->LpD = DIR_BACKWARDS;
				sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
				sc->SmpError = (uint16_t)(0 - sc->SmpError);
			}
		}
	}
}
