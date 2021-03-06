#include <stdlib.h>
#include "test.h"
#define MIDI_DRIVER_INTERNALS
#include "midi/message.h"
#include "midi/port.h"
#include "midi/device.h"
#include "midi/driver.h"

static unsigned char * _buffer = NULL;

static int _send( void * driver, struct MIDIMessage * message ) {
/*struct MIDIDriver * driver = implementation;*/
  size_t size, i;

  ASSERT_NO_ERROR( MIDIMessageGetSize( message, &size ), "Could not determine message size." );
  if( _buffer != NULL ) {
    _buffer = realloc( _buffer, size );
  } else {
    _buffer = malloc( size );
  }
  ASSERT_NOT_EQUAL( _buffer, NULL, "Could not allocate message buffer." );
  ASSERT_NO_ERROR( MIDIMessageEncode( message, size, _buffer, NULL ), "Could not encode message." );
  for( i=0; i<size; i++ ) {
    if( i%16 == 15 || i==size-1 ) {
      printf( "%02x\n", _buffer[i] );
    } else {
      printf( "%02x ", _buffer[i] );
    }
  }
  return 0;
}

static void _destroy( void * driver ) {
  if( _buffer != NULL ) {
    free( _buffer );
    _buffer = NULL;
  }
}

/**
 * Test that a MIDI driver can receive messages.
 */
int test001_driver( void ) {
  struct MIDIPort * port;
  struct MIDIMessage * message;
  struct MIDIDriver * driver;

  MIDIChannel channel = MIDI_CHANNEL_2;
  MIDIKey key = 60;
  MIDIVelocity velocity = 123;    

  message = MIDIMessageCreate( MIDI_STATUS_NOTE_ON );
  ASSERT_NOT_EQUAL( message, NULL, "Could not create note on message!" );
  ASSERT_NO_ERROR( MIDIMessageSet( message, MIDI_CHANNEL, sizeof(MIDIChannel), &channel ),
                   "Could not set message channel." );
  ASSERT_NO_ERROR( MIDIMessageSet( message, MIDI_KEY, sizeof(MIDIKey), &key ),
                   "Could not set message key." );
  ASSERT_NO_ERROR( MIDIMessageSet( message, MIDI_VELOCITY, sizeof(MIDIVelocity), &velocity ),
                   "Could not set message velocity." );
  driver = MIDIDriverCreate( "test driver", MIDI_SAMPLING_RATE_DEFAULT );
  ASSERT_NOT_EQUAL( driver, NULL, "Could not create driver!" );
  driver->send    = &_send;
  driver->destroy = &_destroy;

  ASSERT_NO_ERROR( MIDIDriverGetPort( driver, &port ), "Could not get driver port." );
  ASSERT_NOT_EQUAL( port, NULL, "Could not get driver port!" );

  /* Remember: To "send" message outwards, the driver port (which may be connected to a device)
   *           must *receive* a message! */
  ASSERT_NO_ERROR( MIDIPortReceive( port, MIDIMessageType, message), "Driver port could not receive message." );

  ASSERT_NOT_EQUAL( _buffer, NULL, "Driver did not send the message it received." );
  ASSERT_EQUAL( _buffer[0], MIDI_NIBBLE_VALUE( MIDI_STATUS_NOTE_ON, channel ), "Message status byte incorrectly encoded." );
  ASSERT_EQUAL( _buffer[1], key, "Message key byte incorrectly encoded." );
  ASSERT_EQUAL( _buffer[2], velocity, "Message velocity byte incorrectly encoded." );

  MIDIDriverRelease( driver );
  MIDIMessageRelease( message );
  return 0;
}

/**
 * Test that driver and device work together.
 */
int test002_driver( void ) {
  struct MIDIPort * port;
  struct MIDIDevice * device;
  struct MIDIDriver * driver;
  struct MIDIMessage * message;

  MIDIManufacturerId manufacturer_id = 123;
  unsigned char sysex_data[] = { 1, 2, 3, 5, 8, 13, 21, 34 };
  void * sysex_datap = &sysex_data[0];
  size_t sysex_size = sizeof( sysex_data );
  unsigned char sysex_fragment = 0;

  driver = MIDIDriverCreate( "test driver", MIDI_SAMPLING_RATE_DEFAULT );
  ASSERT_NOT_EQUAL( driver, NULL, "Could not create driver!" );
  device = MIDIDeviceCreate( NULL );
  ASSERT_NOT_EQUAL( device, NULL, "Could not create device!" );

  ASSERT_NO_ERROR( MIDIDriverGetPort( driver, &port ), "Could not get driver port!" );
  ASSERT_NOT_EQUAL( port, NULL, "Provided input port is NULL!" );
  ASSERT_NO_ERROR( MIDIDeviceAttachIn( device, port ), "Could not attach port to device input!" );
  ASSERT_NO_ERROR( MIDIDeviceAttachOut( device, port ), "Could not attach port to device output!" );

  ASSERT_NO_ERROR( MIDIDeviceSendSystemExclusive( device, manufacturer_id, sysex_size, &(sysex_data[0]), sysex_fragment ),
                   "Could not send system exclusive message." );

  message = MIDIMessageCreate( MIDI_STATUS_SYSTEM_EXCLUSIVE );
  ASSERT_NOT_EQUAL( message, NULL, "Could not create system exclusive message." );
  ASSERT_NO_ERROR( MIDIMessageSet( message, MIDI_MANUFACTURER_ID, sizeof(MIDIManufacturerId), &manufacturer_id ),
                   "Could not set message manucaturer id." );
  ASSERT_NO_ERROR( MIDIMessageSet( message, MIDI_SYSEX_FRAGMENT, sizeof(unsigned char), &sysex_fragment ),
                   "Could not set message sysex fragment." );
  ASSERT_NO_ERROR( MIDIMessageSet( message, MIDI_SYSEX_DATA, sizeof(void**), &(sysex_datap) ),
                   "Could not set message sysex data." );
  ASSERT_NO_ERROR( MIDIMessageSet( message, MIDI_SYSEX_SIZE, sizeof(size_t), &(sysex_size) ),
                   "Could not set message sysex size." );

  ASSERT_NO_ERROR( MIDIDeviceDetachOut( device ), "Could not detach connector from device out!" );
  ASSERT_NO_ERROR( MIDIDeviceAttachThru( device, port ), "Could not attach connector to device thru!" );

  ASSERT_NO_ERROR( MIDIDriverReceive( driver, message ), "Could not simulate received message." );

  MIDIMessageRelease( message );
  MIDIDriverRelease( driver );
  MIDIDeviceRelease( device );

  return 0;
}


