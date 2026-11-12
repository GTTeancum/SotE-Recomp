// SPDX-License-Identifier: MIT

// SPDX-FileCopyrightText: 2003 Andy Green
// SPDX-FileCopyrightText: 2004 Craig Edwards
// SPDX-FileCopyrightText: 2005-2006 Richard Osborne
// SPDX-FileCopyrightText: 2006 Guillaume Lamonoca
// SPDX-FileCopyrightText: 2017-2020 Jannik Vogel
// SPDX-FileCopyrightText: 2020-2021 Stefan Schmidt

// Modified for OpenJPB, adapted for SotE: analog PCM only. XEMU implements
// analog AC97 DMA but never drains SO_INDEX (digital output). Polling CIV on
// a dedicated thread keeps SDL supplied without relying on coalesced IRQs.
// Keep this replacement local to the Xbox target.
// Derived from nxdk fb5a9a7a58a431e8d70a9e7da87898059df376c0,
// lib/hal/audio.c, retaining its MIT attribution (licenses/nxdk-audio-MIT.txt).

#include <string.h>
#include <stdbool.h>
#include <hal/audio.h>
#include <xboxkrnl/xboxkrnl.h>
#include <windows.h>
#include <stdint.h>

// The foundation for this file came from the Cromwell audio driver by
// Andy (see his comments below).

// Andy@warmcat.com 2003-03-10:
//
// Xbox PC audio is an AC97 compatible audio controller in the MCPX chip
// http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/24467.pdf
// unlike standard AC97 all the regs appear as MMIO from 0xfec00000
// +0 - +7f = Mixer/Codec regs  <-- Wolfson Micro chip
// +100 - +17f = Busmaster regs

// The Wolfson Micro 9709 Codec fitted to the Xbox is a "dumb" codec that
//  has NO PC-controllable registers.  The only regs it has are the
//  manufacturer and device ID ones.

// S/PDIF comes out of the MCPX device and is controlled by an extra set
//  of busmaster DMA registers at +0x170.  These need their own descriptor
//  separate from the PCM Out, although that descriptor can share audio
//  buffers with PCM out successfully.

static volatile LONG analogBufferCount;
volatile LONG recoveredHalts; /* read by tools via the map */
volatile LONG recoveredBuffers;
static unsigned nextCompletion;
static CRITICAL_SECTION dma_lock;
static int lock_initialized;
volatile uint32_t sote_dma_status[8]; /* polls, queued, consumed, halts, CIV, LVI, SR, pending */

// global reference to the ac97 device
/* AC97 receives one physical address for each 32-entry descriptor ring.
   Keep both rings within a single page so virtual page crossings cannot
   split the DMA table into noncontiguous physical pages. */
AC97_DEVICE ac97Device __attribute__((aligned(4096)));



void XDumpAudioStatus(void)
{
	volatile AC97_DEVICE *pac97device = &ac97Device;
	if (pac97device)
	{
		volatile unsigned char *pb = (unsigned char *)pac97device->mmio;
		//debugPrint("CIV=%02x LVI=%02x SR=%04x CR=%02x\n", pb[0x114], pb[0x115], pb[0x116], pb[0x11B]);
	}
}

// Initialises the audio subsystem.  This *must* be done before
// audio will work.  You can pass NULL as the callback, but if you
// do that, it is your responsibility to keep feeding the data to
// XAudioProvideSamples() manually.
//
// note that I currently ignore sampleSizeInBits and numChannels.  They
// are provided to cope with future enhancements. Currently supported samples
// are 16 bits, 2 channels (stereo)
void XAudioInit(int sampleSizeInBits, int numChannels, XAudioCallback callback, void *data)
{
	if(!lock_initialized){InitializeCriticalSection(&dma_lock);lock_initialized=1;}
	EnterCriticalSection(&dma_lock);
	AC97_DEVICE * pac97device = &ac97Device;

	// Hack to prevent an assertion in MmGetPhysicalAddress by locking the memory.
	// A future API redesign should use proper allocation
	// (MmAllocateContiguousMemory) instead.
	MmLockUnlockBufferPages((PVOID)pac97device, sizeof(AC97_DEVICE), FALSE);

	pac97device->mmio = (unsigned int *)0xfec00000;
	pac97device->nextDescriptor = 0;
	pac97device->callback = callback;
	pac97device->callbackData = data;
	pac97device->sampleSizeInBits = sampleSizeInBits;
	pac97device->numChannels = numChannels;

	volatile unsigned char *pb = (unsigned char *)pac97device->mmio;

	// initialise descriptors to all zero (no samples)
	for(unsigned int i = 0; i < 32; i++) {
		pac97device->pcmSpdifDescriptor[i].bufferStartAddress = 0;
		pac97device->pcmSpdifDescriptor[i].bufferLengthInSamples = 0;
		pac97device->pcmSpdifDescriptor[i].bufferControl = 0;
		pac97device->pcmOutDescriptor[i].bufferStartAddress = 0;
		pac97device->pcmOutDescriptor[i].bufferLengthInSamples = 0;
		pac97device->pcmOutDescriptor[i].bufferControl = 0;
	}

	// perform cold reset
	pac97device->mmio[0x12C>>2] &= ~2;
	LARGE_INTEGER Interval;
	Interval.QuadPart = -10;
	KeDelayExecutionThread(KernelMode, FALSE, &Interval);
	pac97device->mmio[0x12C>>2] |= 2;

	// wait until the chip is finished resetting...
	while(!(pac97device->mmio[0x130>>2]&0x100))
		;

	// reset bus master registers for analog output
	pb[0x11B] = (1 << 4) | (1 << 3) | (1 << 2) | (1 << 1);
	while(pb[0x11B] & (1 << 1))
		;

	// reset bus master registers for digital output
	pb[0x17B] = (1 << 4) | (1 << 3) | (1 << 2) | (1 << 1);
	while(pb[0x17B] & (1 << 1))
		;

	// clear all interrupts
	pb[0x116] = 0xFF;
	pb[0x176] = 0xFF;

	// tell the audio chip where it should look for the descriptors
	unsigned int pcmAddress = (unsigned int)&pac97device->pcmOutDescriptor[0];
	unsigned int spdifAddress = (unsigned int)&pac97device->pcmSpdifDescriptor[0];
	pac97device->mmio[0x100>>2] = 0;  // no PCM input
	pac97device->mmio[0x110>>2] = MmGetPhysicalAddress((void *)pcmAddress);
	pac97device->mmio[0x170>>2] = MmGetPhysicalAddress((void *)spdifAddress);

	// default to being silent...
	XAudioPause();

	// reset buffer status
	analogBufferCount = 0;
	nextCompletion = 0;
	LeaveCriticalSection(&dma_lock);
}

// tell the chip it is OK to play...
void XAudioPlay(void)
{
	AC97_DEVICE *pac97device = &ac97Device;
	volatile unsigned char *pb = (unsigned char *)pac97device->mmio;
	pb[0x11B] = 0x01; // PCM out - run; completions are polled from CIV
	pb[0x17B] = 0; // digital output disabled
}

// tell the chip it is paused.
void XAudioPause(void)
{
	AC97_DEVICE *pac97device = &ac97Device;
	volatile unsigned char *pb = (unsigned char *)pac97device->mmio;
	pb[0x11B] = 0x00; // PCM out - stopped
	pb[0x17B] = 0; // digital output disabled
}

// This is the function you should call when you want to give the
// audio chip some more data.  If you have registered a callback, it
// should call this method.  If you are providing the samples manually,
// you need to make sure you call this function often enough so the
// chip doesn't run out of data
void XAudioProvideSamples(unsigned char *buffer, unsigned short bufferLength, int isFinal)
{
	EnterCriticalSection(&dma_lock);
	AC97_DEVICE *pac97device = &ac97Device;
	volatile unsigned char *pb = (unsigned char *)pac97device->mmio;

	unsigned short bufferControl = 0x8000;
	if (isFinal)
		bufferControl |= 0x4000;

	unsigned int address = MmGetPhysicalAddress((PVOID)buffer);
	unsigned int wordCount = bufferLength / 2;

	pac97device->pcmOutDescriptor[pac97device->nextDescriptor].bufferStartAddress    = address;
	pac97device->pcmOutDescriptor[pac97device->nextDescriptor].bufferLengthInSamples = wordCount;
	pac97device->pcmOutDescriptor[pac97device->nextDescriptor].bufferControl         = bufferControl;
	InterlockedIncrement(&analogBufferCount);
	sote_dma_status[1]++;
	__asm__ volatile("sfence" ::: "memory");
	pb[0x115] = pac97device->nextDescriptor; // publish after accounting

	// increment to the next buffer descriptor (rolling around to 0 once you get to 31)
	pac97device->nextDescriptor = (pac97device->nextDescriptor + 1) % 32;
	LeaveCriticalSection(&dma_lock);
}

// XEMU can halt at the last descriptor without delivering every completion
// interrupt. Reclaim only when the analog engine is definitively halted;
// otherwise SDL's small buffer pool eventually exhausts and audio goes silent.
void xb_audio_dma_service(void)
{
	if(!lock_initialized)return;
	EnterCriticalSection(&dma_lock);
	AC97_DEVICE *device = &ac97Device;
	volatile unsigned char *regs = (unsigned char *)device->mmio;
	if (!device->callback) goto done;
	LONG pending = analogBufferCount;
	if (pending <= 0) goto done;
	unsigned civ = regs[0x114] & 31;
	unsigned status=regs[0x116];
	bool halted=(status&1)!=0;
	/* CIV is still playing unless halted at the last valid buffer (LVBCI).
	   Do not reclaim newly published buffers while a halted DMA restarts. */
	LONG count=(LONG)((civ-nextCompletion)&31);
	if(halted && (status&4))count++;
	sote_dma_status[0]++;sote_dma_status[4]=civ;sote_dma_status[5]=regs[0x115];
	sote_dma_status[6]=status;sote_dma_status[7]=pending;
	if(count<=0 || count>pending)goto done;
	nextCompletion = (nextCompletion + count) & 31;
	InterlockedExchangeAdd(&analogBufferCount, -count);
	sote_dma_status[2]+=count;regs[0x116]=0x1c;
	if (halted) {
		sote_dma_status[3]++;
		InterlockedIncrement(&recoveredHalts);
		InterlockedExchangeAdd(&recoveredBuffers, count);
	}
	while (count-- > 0)
		device->callback((void *)device, device->callbackData);
done:
	LeaveCriticalSection(&dma_lock);
}

void xb_audio_dma_recovery_stats(int *halts, int *buffers)
{
	*halts = recoveredHalts;
	*buffers = recoveredBuffers;
}
