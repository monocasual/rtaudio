/******************************************/
/*
  record.cpp
  by Gary P. Scavone, 2007

  This program records audio from a device and writes it to a
  header-less binary file.  Use the 'playraw', with the same
  parameters and format settings, to playback the audio.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <cstdint>
/*
typedef int8_t MY_TYPE;
#define FORMAT RTAUDIO_SINT8
*/

typedef int16_t MY_TYPE;
#define FORMAT RTAUDIO_SINT16

/*
typedef S24 MY_TYPE;
#define FORMAT RTAUDIO_SINT24

typedef int32_t MY_TYPE;
#define FORMAT RTAUDIO_SINT32

typedef float MY_TYPE;
#define FORMAT RTAUDIO_FLOAT32

typedef double MY_TYPE;
#define FORMAT RTAUDIO_FLOAT64
*/

// Platform-dependent sleep routines.
#if defined( WIN32 )
  #include <windows.h>
  #define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds ) 
#else // Unix variants
  #include <unistd.h>
  #define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#endif

void usage( void ) {
  // Error function in case of incorrect command-line
  // argument specifications
  std::cout << "\nuseage: record N fs <duration> <device> <channelOffset>\n";
  std::cout << "    where N = number of channels,\n";
  std::cout << "    fs = the sample rate,\n";
  std::cout << "    duration = optional time in seconds to record (default = 2.0),\n";
  std::cout << "    device = optional device index to use (default = 0),\n";
  std::cout << "    and channelOffset = an optional channel offset on the device (default = 0).\n\n";
  exit( 0 );
}

unsigned int getDeviceIndex( std::vector<std::string> deviceNames )
{
  unsigned int i;
  std::string keyHit;
  std::cout << '\n';
  for ( i=0; i<deviceNames.size(); i++ )
    std::cout << "  Device #" << i << ": " << deviceNames[i] << '\n';
  do {
    std::cout << "\nChoose a device #: ";
    std::cin >> i;
  } while ( i >= deviceNames.size() );
  std::getline( std::cin, keyHit );  // used to clear out stdin
  return i;
}

struct InputData {
  MY_TYPE* buffer;
  unsigned long bufferBytes;
  unsigned long totalFrames;
  unsigned long frameCounter;
  unsigned int channels;
};

// Interleaved buffers
int input( void * /*outputBuffer*/, void *inputBuffer, unsigned int nBufferFrames,
           double /*streamTime*/, RtAudioStreamStatus /*status*/, void *data )
{
  InputData *iData = (InputData *) data;

  // Simply copy the data to our allocated buffer.
  unsigned int frames = nBufferFrames;
  if ( iData->frameCounter + nBufferFrames > iData->totalFrames ) {
    frames = iData->totalFrames - iData->frameCounter;
    iData->bufferBytes = frames * iData->channels * sizeof( MY_TYPE );
  }

  unsigned long offset = iData->frameCounter * iData->channels;
  memcpy( iData->buffer+offset, inputBuffer, iData->bufferBytes );
  iData->frameCounter += frames;

  if ( iData->frameCounter >= iData->totalFrames ) return 2;
  return 0;
}

int main( int argc, char *argv[] )
{
  unsigned int channels, fs, bufferFrames, device = 0, offset = 0;
  double time = 2.0;
  FILE *fd;

  // minimal command-line checking
  if ( argc < 3 || argc > 6 ) usage();

  RtAudio adc;
  std::vector<unsigned int> deviceIds = adc.getDeviceIds();
  if ( deviceIds.size() < 1 ) {
    std::cout << "\nNo audio devices found!\n";
    exit( 1 );
  }

  channels = (unsigned int) atoi( argv[1] );
  fs = (unsigned int) atoi( argv[2] );
  if ( argc > 3 )
    time = (double) atof( argv[3] );
  if ( argc > 4 )
    device = (unsigned int) atoi( argv[4] );
  if ( argc > 5 )
    offset = (unsigned int) atoi( argv[5] );

  // Let RtAudio print messages to stderr.
  adc.showWarnings( true );

  // Set our stream parameters for input only.
  bufferFrames = 512;
  RtAudio::StreamParameters iParams;
  iParams.nChannels = channels;
  iParams.firstChannel = offset;

  if ( device == 0 )
    iParams.deviceId = adc.getDefaultInputDevice();
  else {
    if ( device >= deviceIds.size() )
      device = getDeviceIndex( adc.getDeviceNames() );
    iParams.deviceId = deviceIds[device];
  }

  InputData data;
  data.buffer = 0;
  if ( adc.openStream( NULL, &iParams, FORMAT, fs, &bufferFrames, &input, (void *)&data ) )
    goto cleanup;

  if ( adc.isStreamOpen() == false ) goto cleanup;

  data.bufferBytes = bufferFrames * channels * sizeof( MY_TYPE );
  data.totalFrames = (unsigned long) (fs * time);
  data.frameCounter = 0;
  data.channels = channels;
  unsigned long totalBytes;
  totalBytes = data.totalFrames * channels * sizeof( MY_TYPE );

  // Allocate the entire data buffer before starting stream.
  data.buffer = (MY_TYPE*) malloc( totalBytes );
  if ( data.buffer == 0 ) {
    std::cout << "Memory allocation error ... quitting!\n";
    goto cleanup;
  }

  if ( adc.startStream() ) goto cleanup;

  std::cout << "\nRecording for " << time << " seconds ... writing file 'record.raw' (buffer frames = " << bufferFrames << ")." << std::endl;
  while ( adc.isStreamRunning() ) {
    SLEEP( 100 ); // wake every 100 ms to check if we're done
  }

  // Now write the entire data to the file.
  fd = fopen( "record.raw", "wb" );
  fwrite( data.buffer, sizeof( MY_TYPE ), data.totalFrames * channels, fd );
  fclose( fd );

 cleanup:
  if ( adc.isStreamOpen() ) adc.closeStream();
  if ( data.buffer ) free( data.buffer );

  return 0;
}
