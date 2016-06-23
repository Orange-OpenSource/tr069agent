#ifdef WIN32
#include <winsock2.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
#else

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <net/if.h>

#endif


#if defined(__sparc__) || defined(WIN32)
#define NOSSL
#endif
#define NOSSL

#include "DM_GlobalDefs.h"
#include "DM_ENG_Parameter.h"
#include "DM_ENG_Common.h"
#include "dm_com_utils.h"
#include "CMN_Trace.h"

#include "dm_lib_udp.h"
#include "dm_lib_stun.h"


/*
 * define the function stunConvertIPAddress only if the trace are
 * activated. 
 */
 /*START STUN INTEGRATION by FT modifications */
// #if FTX_TRACE_LEVEL >= 7

char DBG_addressStr[20];

char *
DBG_stunConvertIPAddress (unsigned int IPnumber)
{
  unsigned int w, x, y, z;
  DBG_addressStr[0] = '\0';
  w = IPnumber & 0x00000000FF;
  x = (IPnumber & 0x000000FF00) >> 8;
  y = (IPnumber & 0x0000FF0000) >> 16;
  z = (IPnumber & 0x00FF000000) >> 24;
  sprintf (DBG_addressStr, "%u.%u.%u.%u", z, y, x, w);
  return DBG_addressStr;
}
// #endif
/*STOP STUN INTEGRATION by FT modifications */

static void computeHmac(u8* hmac, const char* input, int length, const char* key, int keySize);

static bool
stunParseAtrAddress (char* body, unsigned int hdrLen, StunAtrAddress4* result)
{
  if (hdrLen != 8)
  {
    EXEC_ERROR("hdrLen wrong for Address");
    return false;
  }
  result->pad = *body++;
  result->family = *body++;
  if (result->family == IPv4Family)
  {
    UInt16 nport;
    memcpy (&nport, body, 2);
    body += 2;
    result->ipv4.port = ntohs (nport);

    UInt32 naddr;
    memcpy (&naddr, body, 4);
    body += 4;
    result->ipv4.addr = ntohl (naddr);
    return true;
  }
  else if (result->family == IPv6Family)
  {
    EXEC_ERROR("ipv6 not supported");
  }
  else
  {
    EXEC_ERROR("bad address family: %u", result->family);
  }

  return false;
}

static bool
stunParseAtrChangeRequest (char *body, unsigned int hdrLen,
                           StunAtrChangeRequest * result)
{
  if (hdrLen != 4)
  {
    EXEC_ERROR("hdr length = %u expecting %d", hdrLen, sizeof (*result));

    EXEC_ERROR("Incorrect size for ChangeRequest");
    return false;
  }
  else
  {
    memcpy (&result->value, body, 4);
    result->value = ntohl (result->value);
    return true;
  }
}

static bool
stunParseAtrError (char *body, unsigned int hdrLen, StunAtrError * result)
{
  if (hdrLen >= sizeof (*result))
  {
    EXEC_ERROR("head on Error too large");
    return false;
  }
  else
  {
    memcpy (&result->pad, body, 2);
    body += 2;
    result->pad = ntohs (result->pad);
    result->errorClass = *body++;
    result->number = *body++;

    result->sizeReason = hdrLen - 4;
    memcpy (&result->reason, body, result->sizeReason);
    result->reason[result->sizeReason] = 0;
    return true;
  }
}

static bool
stunParseAtrUnknown (char *body, unsigned int hdrLen, StunAtrUnknown * result)
{
  int i = 0;
  if (hdrLen >= sizeof (*result))
  {
    return false;
  }
  else
  {
    if (hdrLen % 4 != 0)
      return false;
    result->numAttributes = hdrLen / 4;
    for (i = 0; i < result->numAttributes; i++)
    {
      memcpy (&result->attrType[i], body, 2);
      body += 2;
      result->attrType[i] = ntohs (result->attrType[i]);
    }
    return true;
  }
}


static bool
stunParseAtrString (char *body, unsigned int hdrLen, StunAtrString * result)
{
  if (hdrLen >= STUN_MAX_STRING)
  {
    EXEC_ERROR("String is too large");
    return false;
  }
  else
  {
    if (hdrLen % 4 != 0)
    {
      EXEC_ERROR("Bad length string %u", hdrLen);
      return false;
    }

    result->sizeValue = hdrLen;
    memcpy (&result->value, body, hdrLen);
    result->value[hdrLen] = 0;
    return true;
  }
}


static bool
stunParseAtrIntegrity(char *body, unsigned int hdrLen, StunAtrIntegrity * result)
{
  if (hdrLen != 20)
  {
    EXEC_ERROR("MessageIntegrity must be 20 bytes");
    return false;
  }
  else
  {
    memcpy (&result->hash, body, hdrLen);
    int i;
    printf("\nHEXA\n"); 
    for(i=0; i < 10;i++) {
     printf("%.2X", body[i]);
    }
    printf("\nCARACTERE\n");

    //for(i=0; i < 10;i++ ) {
    // printf("%c", body[i]);
    //}
    
    printf("\n\n");
 
    return true;
  }
}


bool
stunParseMessage (char *buf, unsigned int bufLen, StunMessage * msg)
{
  DBG ("StunParseMessage");

  DBG ("Received stun message: %u bytes", bufLen);
  memset (msg, 0, sizeof (*msg));

  if (sizeof (StunMsgHdr) > bufLen)
  {
    EXEC_ERROR("Bad message");
    return false;
  }

  memcpy (&msg->msgHdr, buf, sizeof (StunMsgHdr));
  msg->msgHdr.msgType = ntohs (msg->msgHdr.msgType);
  msg->msgHdr.msgLength = ntohs (msg->msgHdr.msgLength);

  if (msg->msgHdr.msgLength + sizeof (StunMsgHdr) != bufLen)
  {
    EXEC_ERROR("Message header length doesn't match message size: %u - %u",
           msg->msgHdr.msgLength, bufLen);
    return false;
  }

  char *body = buf + sizeof (StunMsgHdr);
  unsigned int size = msg->msgHdr.msgLength;

  // clog << "bytes after header = " << size << endl;

  while (size > 0)
  {
    // !jf! should check that there are enough bytes left in the
    // buffer

    StunAtrHdr *attr =          /* reinterpret_cast<StunAtrHdr*> */
      (StunAtrHdr *) (body);

    unsigned int attrLen = ntohs (attr->length);
    int atrType = ntohs (attr->type);

    // if (verbose) clog << "Found attribute type=" <<
    // AttrNames[atrType] << " length=" << attrLen << endl;
    if (attrLen + 4 > size)
    {
      EXEC_ERROR
        ("claims attribute is larger than size of message (attribute type=%d)",
         atrType);
      DBG("attrLen=%u size=%u",attrLen,size);
      return false;
    }

    body += 4;                  // skip the length and type in attribute
    // header
    size -= 4;

    switch (atrType)
    {
    case MappedAddress:
      msg->hasMappedAddress = true;
      if (stunParseAtrAddress (body, attrLen, &msg->mappedAddress) == false)
      {
        EXEC_ERROR("problem parsing MappedAddress");
        return false;
      }
      else
      {
        DBG ("MappedAddress = %s:%u",
             DBG_stunConvertIPAddress ((unsigned int) msg->
                                       mappedAddress.ipv4.addr),
             msg->mappedAddress.ipv4.port);
      }

      break;

    case ResponseAddress:
      msg->hasResponseAddress = true;
      if (stunParseAtrAddress (body, attrLen, &msg->responseAddress) == false)
      {
        EXEC_ERROR("problem parsing ResponseAddress");
        return false;
      }
      else
      {
        DBG ("ResponseAddress = %s:%u",
             DBG_stunConvertIPAddress ((unsigned int) msg->
                                       responseAddress.ipv4.addr),
             msg->responseAddress.ipv4.port);
      }
      break;

    case ChangeRequest:
      msg->hasChangeRequest = true;
      if (stunParseAtrChangeRequest
          (body, attrLen, &msg->changeRequest) == false)
      {
        EXEC_ERROR("problem parsing ChangeRequest");
        return false;
      }
      else
      {
        DBG ("ChangeRequest = %u", msg->changeRequest.value);
      }
      break;

    case SourceAddress:
      msg->hasSourceAddress = true;
      if (stunParseAtrAddress (body, attrLen, &msg->sourceAddress) == false)
      {
        EXEC_ERROR("problem parsing SourceAddress");
        return false;
      }
      else
      {

        DBG ("SourceAddress = %s:%u",
             DBG_stunConvertIPAddress ((unsigned int) msg->
                                       sourceAddress.ipv4.addr),
             msg->sourceAddress.ipv4.port);
      }
      break;

    case ChangedAddress:
      msg->hasChangedAddress = true;
      if (stunParseAtrAddress (body, attrLen, &msg->changedAddress) == false)
      {
        EXEC_ERROR("problem parsing ChangedAddress");
        return false;
      }
      else
      {
        DBG ("ChangedAddress = %s:%u",
             DBG_stunConvertIPAddress ((unsigned int) msg->
                                       changedAddress.ipv4.addr),
             msg->changedAddress.ipv4.port);
      }
      break;

    case Username:
      msg->hasUsername = true;
      if (stunParseAtrString (body, attrLen, &msg->username) == false)
      {
        EXEC_ERROR("problem parsing Username");
        return false;
      }
      else
      {
        DBG ("Username = %s", msg->username.value);
      }

      break;

    case Password:
      msg->hasPassword = true;
      if (stunParseAtrString (body, attrLen, &msg->password) == false)
      {
        EXEC_ERROR("problem parsing Password");
        return false;
      }
      else
      {
        DBG ("Password = %s", msg->password.value);
      }
      break;

    case MessageIntegrity:
      msg->hasMessageIntegrity = true;
      if (stunParseAtrIntegrity(body, attrLen, &msg->messageIntegrity) == false)
      {
        EXEC_ERROR("problem parsing MessageIntegrity");
        return false;
      }
      // read the current HMAC
      // look up the password given the user of given the
      // transaction id 
      // compute the HMAC on the buffer
      // decide if they match or not
      break;

    case ErrorCode:
      msg->hasErrorCode = true;
      if (stunParseAtrError (body, attrLen, &msg->errorCode) == false)
      {
        EXEC_ERROR("problem parsing ErrorCode");
        return false;
      }
      else
      {
        DBG ("ErrorCode = %d %d %s", (int) msg->errorCode.errorClass, (int) msg->errorCode.number, msg->errorCode.reason);
      }

      break;

    case UnknownAttribute:
      msg->hasUnknownAttributes = true;
      if (stunParseAtrUnknown (body, attrLen, &msg->unknownAttributes)
          == false)
      {
        EXEC_ERROR("problem parsing UnknownAttribute");
        return false;
      }
      break;

    case ReflectedFrom:
      msg->hasReflectedFrom = true;
      if (stunParseAtrAddress (body, attrLen, &msg->reflectedFrom) == false)
      {
        EXEC_ERROR("problem parsing ReflectedFrom");
        return false;
      }
      break;

    case XorMappedAddress:
      msg->hasXorMappedAddress = true;
      if (stunParseAtrAddress (body, attrLen, &msg->xorMappedAddress)
          == false)
      {
        EXEC_ERROR("problem parsing XorMappedAddress");
        return false;
      }
      else
      {
        DBG ("XorMappedAddress = %s:%u",
             DBG_stunConvertIPAddress ((unsigned int) msg->
                                       mappedAddress.ipv4.addr),
             msg->mappedAddress.ipv4.port);
      }
      break;

    case XorOnly:
      msg->xorOnly = true;
      DBG ("xorOnly = true");
      break;

    case ServerName:
      msg->hasServerName = true;
      if (stunParseAtrString (body, attrLen, &msg->serverName) == false)
      {
        EXEC_ERROR("problem parsing ServerName");
        return false;
      }
      else
      {
        DBG ("ServerName = %s", msg->serverName.value);
      }
      break;

    case SecondaryAddress:
      msg->hasSecondaryAddress = true;
      if (stunParseAtrAddress (body, attrLen, &msg->secondaryAddress)
          == false)
      {
        EXEC_ERROR("problem parsing secondaryAddress");
        return false;
      }
      else
      {
         DBG ("SecondaryAddress = %s%u",
             DBG_stunConvertIPAddress ((unsigned int) msg->
                                       secondaryAddress.ipv4.addr),
             msg->secondaryAddress.ipv4.port);
      }
      break;

      /*START STUN INTEGRATION by FT modifications */
      case CONNECTION_REQUEST_BINDING:
        msg->hasConnectionRequestBinding=true;
      break;

      case BINGIND_CHANGE:
        msg->hasBindingChange=true;
      break;
      /*END STUN INTEGRATION by FT modifications */

    default:
      EXEC_ERROR("Unknown attribute: %d", atrType);
      if (atrType <= 0x7FFF)
      {
        return false;
      }
    }

    body += attrLen;
    size -= attrLen;
  }

  return true;
}


char *
encode16 (char *buf, UInt16 data)
{
  UInt16 ndata = htons (data);
  memcpy (buf, (void *) (&ndata), sizeof (UInt16));
  return buf + sizeof (UInt16);
}

char *
encode32 (char *buf, UInt32 data)
{
  UInt32 ndata = htonl (data);
  memcpy (buf, (void *) (&ndata), sizeof (UInt32));
  return buf + sizeof (UInt32);
}


char *
encode (char *buf, const void *data, unsigned int length)
{
  memcpy (buf, data, length);
  return buf + length;
}


static char *
encodeAtrAddress4 (char *ptr, UInt16 type, const StunAtrAddress4 * atr)
{
  ptr = encode16 (ptr, type);
  ptr = encode16 (ptr, 8); //8xUInt8 = 64 bytes
  *ptr++ = atr->pad; //UInt8
  *ptr++ = IPv4Family; //UInt8
  ptr = encode16 (ptr, atr->ipv4.port);
  ptr = encode32 (ptr, atr->ipv4.addr);

  return ptr;
}

static char *
encodeAtrChangeRequest (char *ptr, const StunAtrChangeRequest * atr)
{
  ptr = encode16 (ptr, ChangeRequest);
  ptr = encode16 (ptr, 4);
  ptr = encode32 (ptr, atr->value);
  return ptr;
}

static char *
encodeAtrError (char *ptr, const StunAtrError * atr)
{
  DBG("reason %s",atr->reason);
  DBG("size of the reason %u",atr->sizeReason);

  ptr = encode16 (ptr, ErrorCode); //type : 16
  ptr = encode16 (ptr, 4 +  atr->sizeReason);  //length : 16
  ptr = encode16 (ptr, atr->pad); // (21) 16 bytes to 0
  *ptr++ = atr->errorClass; //UInt8 (3 bytes)
  *ptr++ = atr->number;  //UInt8 (8 bytes)
  ptr = encode (ptr, atr->reason, atr->sizeReason);
  return ptr;
}


static char *
encodeAtrUnknown (char *ptr, const StunAtrUnknown * atr)
{
  int i = 0;
  ptr = encode16 (ptr, UnknownAttribute);
  ptr = encode16 (ptr, 2 + 2 * atr->numAttributes);
  for (i = 0; i < atr->numAttributes; i++)
  {
    ptr = encode16 (ptr, atr->attrType[i]);
  }
  return ptr;
}


static char *
encodeXorOnly (char *ptr)
{
  ptr = encode16 (ptr, XorOnly);
  return ptr;
}


static char *
encodeAtrString (char *ptr, UInt16 type, const StunAtrString * atr)
{
  assert (atr->sizeValue % 4 == 0);

  ptr = encode16 (ptr, type);
  ptr = encode16 (ptr, atr->sizeValue);
  ptr = encode (ptr, atr->value, atr->sizeValue);
  return ptr;
}


static char *
encodeAtrIntegrity (char *ptr, const StunAtrIntegrity * atr)
{
  ptr = encode16 (ptr, MessageIntegrity);
  ptr = encode16 (ptr, 20);
  ptr = encode (ptr, atr->hash, sizeof (atr->hash));
  return ptr;
}

static char *
encodeBindingChange (char *ptr)
{
  // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // | Type | Length |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // | Value ....
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  ptr = encode16 (ptr, BINGIND_CHANGE);
  ptr = encode16 (ptr, 0);
  return ptr;
}

unsigned int
stunEncodeMessage (const StunMessage* msg, char* buf, unsigned int bufLen, const StunAtrString* password)
{
  assert (bufLen >= sizeof (StunMsgHdr));
  char *ptr = buf;

  ptr = encode16 (ptr, msg->msgHdr.msgType);
  char *lengthp = ptr;
  ptr = encode16 (ptr, 0);
  ptr = encode (ptr, (const char *) (msg->msgHdr.id.octet), sizeof (msg->msgHdr.id) );

  DBG ("Encoding stun message:");
  if (msg->hasMappedAddress)
  {
    DBG ("Encoding MappedAddress: %s:%u",
         DBG_stunConvertIPAddress (msg->mappedAddress.ipv4.addr),
         msg->mappedAddress.ipv4.port);
    ptr = encodeAtrAddress4 (ptr, MappedAddress, &msg->mappedAddress);
  }

  if (msg->hasResponseAddress)
  {
    DBG ("Encoding ResponseAddress: %s:%u",
         DBG_stunConvertIPAddress (msg->responseAddress.ipv4.addr),
         msg->responseAddress.ipv4.port);
    ptr = encodeAtrAddress4 (ptr, ResponseAddress, &msg->responseAddress);
  }

  if (msg->hasChangeRequest)
  {
    DBG ("Encoding ChangeRequest: %u", msg->changeRequest.value);
    ptr = encodeAtrChangeRequest (ptr, &msg->changeRequest);
  }

  if (msg->hasSourceAddress)
  {
    DBG ("Encoding SourceAddress: %s:%u",
         DBG_stunConvertIPAddress (msg->sourceAddress.ipv4.addr),
         msg->sourceAddress.ipv4.port);
    ptr = encodeAtrAddress4 (ptr, SourceAddress, &msg->sourceAddress);
  }

  if (msg->hasChangedAddress)
  {
    DBG ("Encoding ChangedAddress: %s:%u",
         DBG_stunConvertIPAddress (msg->changedAddress.ipv4.addr),
         msg->changedAddress.ipv4.port);
    ptr = encodeAtrAddress4 (ptr, ChangedAddress, &msg->changedAddress);
  }

  if (msg->hasUsername)
  {
    DBG ("Encoding Username: %s", msg->username.value);
    ptr = encodeAtrString (ptr, Username, &msg->username);
  }

  if (msg->hasPassword)
  {
    DBG ("Encoding Password: %s", msg->password.value);
    ptr = encodeAtrString (ptr, Password, &msg->password);
  }

  if (msg->hasErrorCode)
  {
    DBG ("Encoding ErrorCode: class= %d number= %d reason= %s",
         (int) msg->errorCode.errorClass, (int) msg->errorCode.number,
         msg->errorCode.reason);
    ptr = encodeAtrError (ptr, &msg->errorCode);
  }

  if (msg->hasUnknownAttributes)
  {
    DBG ("Encoding UnknownAttribute: ???");
    ptr = encodeAtrUnknown (ptr, &msg->unknownAttributes);
  }

  if (msg->hasReflectedFrom)
  {
    DBG ("Encoding ReflectedFrom: %s:%u",
         DBG_stunConvertIPAddress (msg->reflectedFrom.ipv4.addr),
         msg->reflectedFrom.ipv4.port);
    ptr = encodeAtrAddress4 (ptr, ReflectedFrom, &msg->reflectedFrom);
  }

  if (msg->hasXorMappedAddress)
  {
    DBG ("Encoding XorMappedAddress: %s:%u",
         DBG_stunConvertIPAddress (msg->xorMappedAddress.ipv4.addr),
         msg->xorMappedAddress.ipv4.port);
    ptr = encodeAtrAddress4 (ptr, XorMappedAddress, &msg->xorMappedAddress);
  }

  if (msg->xorOnly)
  {
    DBG ("Encoding xorOnly:");
    ptr = encodeXorOnly (ptr);
  }

  if (msg->hasServerName)
  {
    DBG ("Encoding ServerName: %s", msg->serverName.value);
    ptr = encodeAtrString (ptr, ServerName, &msg->serverName);
  }

  if (msg->hasSecondaryAddress)
  {
    DBG ("Encoding SecondaryAddress: %s:%u",
         DBG_stunConvertIPAddress (msg->secondaryAddress.ipv4.addr),
         msg->secondaryAddress.ipv4.port);
    ptr = encodeAtrAddress4 (ptr, SecondaryAddress, &msg->secondaryAddress);
  }
  /*START STUN INTEGRATION by FT modifications */
  if (msg->hasConnectionRequestBinding)
  {
    DBG ("Encoding CONNECTION_REQUEST_BINDING");
    ptr=encodeAtrString(ptr, CONNECTION_REQUEST_BINDING,
                       &msg->connectionRequestBinding);
  }

  if (msg->hasBindingChange)
  {
    DBG ("Encoding BINDING_CHANGE");
    ptr = encodeBindingChange (ptr);
  }

  UInt16 sizeHeader=(UInt16) (ptr - buf - sizeof (StunMsgHdr));
  if (password->sizeValue > 0)
  {
    //add the HMAC header and value length
    sizeHeader += 2*sizeof(UInt16)+sizeof(StunAtrIntegrity);
  }
  encode16 (lengthp, sizeHeader);

  if (password->sizeValue > 0)
  {
    DBG ("HMAC with password: %s (size=%d) ", password->value,password->sizeValue);
    StunAtrIntegrity integrity;

    computeHmac (integrity.hash, buf, (int) (ptr - buf),  password->value, password->sizeValue);

    ptr = encodeAtrIntegrity (ptr, &integrity);
  }
  /*STOP STUN INTEGRATION by FT modifications */
  return (int) (ptr - buf);
}

int
stunRand ()
{
  // return 32 bits of random stuff
  assert (sizeof (int) == 4);
  static bool init = false;
  if (!init)
  {
    init = true;

    UInt64 tick;

#if defined(WIN32)
    volatile unsigned int lowtick = 0, hightick = 0;
    __asm
    {
    rdtsc mov lowtick, eax mov hightick, edx} tick = hightick;
    tick <<= 32;
    tick |= lowtick;
#elif defined(__GNUC__) && ( defined(__i686__) || defined(__i386__) || defined(__x86_64__) )
  asm ("rdtsc":"=A" (tick));
#elif defined (__SUNPRO_CC) || defined( __sparc__ )
    tick = gethrtime ();
#elif SH4-LINUX || mips
    int fd = open ("/dev/random", O_RDONLY);
    read (fd, &tick, sizeof (tick));
    close (fd);
#elif defined(__MACH__)
    int fd = open ("/dev/random", O_RDONLY);
    read (fd, &tick, sizeof (tick));
    close (fd);
#else
#if defined (bc1161)
    /*
     * srand(rand()); tick = rand();
     */
    tick = time (0);
#else
    error Need some way to seed the random number generator
#endif
#endif
    int seed = (int) (tick);
#ifdef WIN32
    srand (seed);
#else
    srandom (seed);
#endif
  }
#ifdef WIN32
  assert (RAND_MAX == 0x7fff);
  int r1 = rand ();
  int r2 = rand ();

  int ret = (r1 << 16) + r2;

  return ret;
#else
  return random ();
#endif
}


// / return a random number to use as a port 
int
stunRandomPort ()
{
  int min = 0x4000;
  int max = 0x7FFF;

  int ret = stunRand ();
  ret = ret | min;
  ret = ret & max;

  return ret;
}

#ifdef NOSSL
  /*START STUN INTEGRATION by FT modifications */
static void computeHmac(u8* hmac, const char* input, int length, const char* key, int sizeKey)
{
	hmac_sha1 ( (u8*)key, sizeKey, (u8 *) input, length, hmac);
}
  /*STOP STUN INTEGRATION by FT modifications */
#else
#include <openssl/hmac.h>

static void
computeHmac (char *hmac, const char *input, int length, const char *key,
             int sizeKey)
{
  unsigned int resultSize = 0;
  HMAC (EVP_sha1 (),
        key, sizeKey,
        reinterpret_cast < const unsigned char *>(input), length,
        reinterpret_cast < unsigned char *>(hmac), &resultSize);
  assert (resultSize == 20);
}
#endif


static void
toHex (const u8 * buffer, int bufferSize, char *output)
{
  static char hexmap[] = "0123456789abcdef";
  int i = 0;
  const u8 *p = buffer;
  char *r = output;
  for (i = 0; i < bufferSize; i++)
  {
    unsigned char temp = *p++;

    int hi = (temp & 0xf0) >> 4;
    int low = (temp & 0xf);

    *r++ = hexmap[hi];
    *r++ = hexmap[low];
  }
  *r = 0;
}

void
stunCreateUserName (const StunAddress4 * source, StunAtrString * username)
{
  UInt64 time = stunGetSystemTimeSecs ();
  time -= (time % 20 * 60);
  // UInt64 hitime = time >> 32;
  UInt64 lotime = time & 0xFFFFFFFF;

  char buffer[1024];
  sprintf (buffer,
           "%08x:%08x:%08x:",
           (UInt32) (source->addr),
           (UInt32) (stunRand ()), (UInt32) (lotime));
  assert (strlen (buffer) < 1024);

  assert (strlen (buffer) + 41 < STUN_MAX_STRING);

  u8 hmac[20];
  char key[] = "Jason";
  computeHmac (hmac, buffer, strlen (buffer), key, strlen (key));
  char hmacHex[41];
  toHex (hmac, 20, hmacHex);
  hmacHex[40] = 0;

  strcat (buffer, hmacHex);

  int l = strlen (buffer);
  assert (l + 1 < STUN_MAX_STRING);
  assert (l % 4 == 0);

  username->sizeValue = l;
  memcpy (username->value, buffer, l);
  username->value[l] = 0;

  // if (verbose) clog << "computed username=" << username.value <<
  // endl;
}

void
stunCreatePassword (const StunAtrString * username, StunAtrString * password)
{
  u8 hmac[20];
  char key[] = "1234";

  computeHmac (hmac, username->value, strlen (username->value), key,
               strlen (key));
  toHex (hmac, 20, password->value);
  password->sizeValue = 40;
  password->value[40] = 0;

  // clog << "password=" << password->value << endl;
}


UInt64
stunGetSystemTimeSecs ()
{
  UInt64 time = 0;
#if defined(WIN32)
  SYSTEMTIME t;
  // CJ TODO - this probably has bug on wrap around every 24 hours
  GetSystemTime (&t);
  time = (t.wHour * 60 + t.wMinute) * 60 + t.wSecond;
#else
  struct timeval now;
  gettimeofday (&now, NULL);
  // assert( now );
  time = now.tv_sec;
#endif
  return time;
}

/*
 * ostream& operator<< ( ostream& strm, const UInt128& r ) { strm <<
 * int(r.octet[0]); for ( int i=1; i<16; i++ ) { strm << ':' <<
 * int(r.octet[i]); }
 * 
 * return strm; }
 * 
 * ostream& operator<<( ostream& strm, const StunAddress4& addr) { UInt32 
 * ip = addr.addr; strm << ((int)(ip>>24)&0xFF) << "."; strm <<
 * ((int)(ip>>16)&0xFF) << "."; strm << ((int)(ip>> 8)&0xFF) << "."; strm
 * << ((int)(ip>> 0)&0xFF) ;
 * 
 * strm << ":" << addr.port;
 * 
 * return strm; }
 */


// returns true if it scucceeded
bool
stunParseHostName (char *peerName,
                   UInt32 * ip, UInt16 * portVal, UInt16 defaultPort)
{
  struct in_addr sin_addr;

  char host[512];
  strncpy (host, peerName, 512);
  host[512 - 1] = '\0';
  char *port = NULL;

  int portNum = defaultPort;

  // pull out the port part if present.
  char *sep = strchr (host, ':');

  if (sep == NULL)
  {
    portNum = defaultPort;
  }
  else
  {
    *sep = '\0';
    port = sep + 1;
    // set port part

    char *endPtr = NULL;

    portNum = strtol (port, &endPtr, 10);

    if (endPtr != NULL)
    {
      if (*endPtr != '\0')
      {
        portNum = defaultPort;
      }
    }
  }

  if (portNum < 1024)
    return false;
  if (portNum >= 0xFFFF)
    return false;

  // figure out the host part 
  struct hostent *h;

#ifdef WIN32
  assert (strlen (host) >= 1);
  if (isdigit (host[0]))
  {
    // assume it is a ip address 
    unsigned long a = inet_addr (host);
    // cerr << "a=0x" << hex << a << dec << endl;

    *ip = ntohl (a);
  }
  else
  {
    // assume it is a host name 
    h = gethostbyname (host);

    if (h == NULL)
    {
      int err = errno;
      EXEC_ERROR("error was %d", err);
      assert (err != WSANOTINITIALISED);

      *ip = ntohl (0x7F000001L);

      return false;
    }
    else
    {
      sin_addr = *(struct in_addr *) h->h_addr;
      *ip = ntohl (sin_addr.s_addr);
    }
  }

#else
  h = gethostbyname (host);
  if (h == NULL)
  {
    EXEC_ERROR("error was %d", (int) errno);
    *ip = ntohl (0x7F000001L);
    return false;
  }
  else
  {
    sin_addr = *(struct in_addr *) h->h_addr;
    *ip = ntohl (sin_addr.s_addr);
  }
#endif

  *portVal = portNum;

  return true;
}


bool
stunParseServerName (char *name, StunAddress4 * addr)
{
  assert (name);

  // TODO - put in DNS SRV stuff.

  bool ret = stunParseHostName (name, &addr->addr, &addr->port, 3478);
  if (ret != true)
  {
    addr->port = 0xFFFF;
  }
  return ret;
}


static void
stunCreateErrorResponse(StunMessage * response, int cl, int number,
                         const char *msg)
{
  response->msgHdr.msgType = BindErrorResponseMsg;
  response->hasErrorCode = true;
  response->errorCode.errorClass = cl;
  response->errorCode.number = number;
  response->errorCode.sizeReason=strlen(msg);
  strcpy (response->errorCode.reason, msg);
}

#if 0
static void
stunCreateSharedSecretErrorResponse (StunMessage & response, int cl,
                                     int number, const char *msg)
{
  response.msgHdr.msgType = SharedSecretErrorResponseMsg;
  response.hasErrorCode = true;
  response.errorCode.errorClass = cl;
  response.errorCode.number = number;
  strcpy (response.errorCode.reason, msg);
}
#endif

static void
stunCreateSharedSecretResponse (const StunMessage * request,
                                const StunAddress4 * source,
                                StunMessage * response)
{
  response->msgHdr.msgType = SharedSecretResponseMsg;
  response->msgHdr.id = request->msgHdr.id;

  response->hasUsername = true;
  stunCreateUserName (source, &response->username);

  response->hasPassword = true;
  stunCreatePassword (&response->username, &response->password);
}

// This funtion takes a single message sent to a stun server, parses
// and constructs an apropriate repsonse - returns true if message is
// valid


bool
stunServerProcessMsg (char *buf,
                      unsigned int bufLen,
                      StunAddress4 * from,
                      StunAddress4 * secondary UNUSED,
                      StunAddress4 * myAddr,
                      StunAddress4 * altAddr UNUSED,
                      StunMessage * resp,
                      StunAddress4 * destination,
                      StunAtrString * hmacPassword,
                      bool * changePort, bool * changeIp,
                      int natTimeSession)
{
  /*START STUN INTEGRATION by FT modifications */
  bool err=false;

  static int g_timeLastBinding=0;
  static UInt16 g_fromPort=0;

  // set up information for default response 

  memset (resp, 0, sizeof (*resp));

  *changeIp = false;
  *changePort = false;

  StunMessage req;
  bool ok = stunParseMessage (buf, bufLen, &req);

  if (!ok)                      // Complete garbage, drop it on the floor
  {
    DBG ("Request did not parse");
    return false;
  }
  DBG ("Request parsed ok");

  /*StunAddress4 mapped = req.mappedAddress.ipv4;
  StunAddress4 respondTo = req.responseAddress.ipv4;
  UInt32 flags = req.changeRequest.value;*/

  switch (req.msgHdr.msgType)
  {
  // ------------------------------------------------------------------------------------
  case SharedSecretRequestMsg:
    INFO("Received SharedSecretRequestMsg on udp. send error 433.");
    // !cj! - should fix so you know if this came over TLS or UDP
    stunCreateSharedSecretResponse (&req, from, resp);
    // stunCreateSharedSecretErrorResponse(*resp, 4, 33, "this request 
    // must be over TLS");
    err=true;
  break;
  // ------------------------------------------------------------------------------------
  case BindRequestMsg:
    if(! req.hasConnectionRequestBinding)
    {
      INFO("TIMEOUT_DISCOVERY received");
      struct timezone tz;
      struct timeval tv;
      gettimeofday(&tv, &tz);

      if(g_fromPort != from->port)
      {
        INFO("new address : then new delay");
        g_timeLastBinding=tv.tv_sec;
        g_fromPort=from->port;
      }
      else
      {
        INFO("*** Time between two timeouts=%lds ***",(tv.tv_sec-g_timeLastBinding));
        if( (tv.tv_sec-g_timeLastBinding) >= natTimeSession )
        {
          (from->port)++;
          INFO("simuling the timeout nat session");
        }
        else
        {
          g_timeLastBinding=tv.tv_sec;
        }
      }

      INFO("TIMEOUT_RESPONSE response will be sended");
      resp->hasMappedAddress=true;
      resp->hasSourceAddress=true;
      resp->hasChangedAddress=true;
      resp->hasReflectedFrom=true;

      resp->reflectedFrom.ipv4.addr=from->addr;
      resp->reflectedFrom.ipv4.port=from->port;

      destination->addr = req.responseAddress.ipv4.addr;
      destination->port = req.responseAddress.ipv4.port;
      DBG("from ip=%s port=%u",DBG_stunConvertIPAddress(from->addr),from->port);
      DBG("responseAdress ip=%s port=%u",
          DBG_stunConvertIPAddress(req.responseAddress.ipv4.addr),
          req.responseAddress.ipv4.port);
    }
    else
    {
      DBG("from ip=%s port=%u",DBG_stunConvertIPAddress(from->addr),from->port);
      DBG("responseAdress ip=%s port=%u",
          DBG_stunConvertIPAddress(req.responseAddress.ipv4.addr),
                                   req.responseAddress.ipv4.port);
      destination->addr = from->addr;
      destination->port = from->port;

      if(req.hasBindingChange && req.hasUsername && ! req.hasMessageIntegrity)
      {
        INFO("BINDING_CHANGE received");
        INFO("BINDING_ERROR_RESPONSE will be sended");
        stunCreateErrorResponse (resp, 4, 1, "Unauthorized");
        return true;
      }
      else if(req.hasBindingChange && req.hasUsername && req.hasMessageIntegrity)
      {
        INFO("MSG_INTEGRITY received");
        INFO("MSG_INTEGRITY_RESPONSE will be sended");
        resp->hasMappedAddress=true;
        resp->hasSourceAddress=true;
        resp->hasChangedAddress=true;
        resp->hasMessageIntegrity=true;
        strcpy(hmacPassword->value,"1234");
        hmacPassword->sizeValue=4;
      }
      else if(req.hasUsername)
      {
        INFO("BINDING_REQUEST received");
        INFO("BINDING_RESPONSE will be sended");
        resp->hasMappedAddress=true;
        resp->hasSourceAddress=true;
        resp->hasChangedAddress=true;
      }
    }

    err=true;
  break;

  // ------------------------------------------------------------------------------------
  default:
    EXEC_ERROR("Unknown or unsupported request");
  }

  if(resp->hasMappedAddress)
  {
    resp->mappedAddress.ipv4.addr=from->addr;
    resp->mappedAddress.ipv4.port=from->port;
  }

  if(resp->hasSourceAddress)
  {
    resp->sourceAddress.ipv4.addr=myAddr->addr;
    resp->sourceAddress.ipv4.port=myAddr->port;
  }

  if(resp->hasChangedAddress)
  {
    resp->changedAddress.ipv4.addr=myAddr->addr;
    resp->changedAddress.ipv4.port=myAddr->port;
  }

   // form the outgoing message
  resp->msgHdr.msgType = BindResponseMsg;
  int i = 0;
  for (i = 0; i < 16; i++)
  {
    resp->msgHdr.id.octet[i] = req.msgHdr.id.octet[i];
  }

  return err;
  /*STOP STUN INTEGRATION by FT modifications */
}

bool
stunInitServer (StunServerInfo * info, const StunAddress4 * myAddr,
                const StunAddress4 * altAddr, int startMediaPort)
{
  int i = 0;
  assert (myAddr->port != 0);
  assert (altAddr->port != 0);
  assert (myAddr->addr != 0);
  // assert( altAddr.addr != 0 );

  DBG ("function : %s, line : %d", __FUNCTION__, __LINE__);
  info->myAddr = *myAddr;
  info->altAddr = *altAddr;

  info->myFd = INVALID_SOCKET;
  info->altPortFd = INVALID_SOCKET;
  info->altIpFd = INVALID_SOCKET;
  info->altIpPortFd = INVALID_SOCKET;

  memset (info->relays, 0, sizeof (info->relays));
  if (startMediaPort > 0)
  {
    info->relay = true;

    for (i = 0; i < MAX_MEDIA_RELAYS; ++i)
    {
      StunMediaRelay *relay = &info->relays[i];
      relay->relayPort = startMediaPort + i;
      relay->fd = 0;
      relay->expireTime = 0;
    }
  }
  else
  {
    info->relay = false;
  }

  if ((info->myFd = openPort (myAddr->port, myAddr->addr)) == INVALID_SOCKET)
  {
    EXEC_ERROR("Can't open %s:%u", DBG_stunConvertIPAddress (myAddr->addr),
           myAddr->port);
    stunStopServer (info);

    return false;
  }
  // if (verbose) clog << "Opened " << myAddr.addr << ":" << myAddr.port 
  // << " --> " << info.myFd << endl;

  if ((info->altPortFd =
       openPort (altAddr->port, myAddr->addr)) == INVALID_SOCKET)
  {
    EXEC_ERROR("Can't open %s:%u",
           DBG_stunConvertIPAddress (myAddr->addr), myAddr->port);

    stunStopServer (info);
    return false;
  }
  // if (verbose) clog << "Opened " << myAddr.addr << ":" <<
  // altAddr.port << " --> " << info.altPortFd << endl;


  info->altIpFd = INVALID_SOCKET;
  if (altAddr->addr != 0)
  {
    if ((info->altIpFd =
         openPort (myAddr->port, altAddr->addr)) == INVALID_SOCKET)
    {
      EXEC_ERROR("Can't open %s:%u",
             DBG_stunConvertIPAddress (altAddr->addr), altAddr->port);

      stunStopServer (info);
      return false;
    }
    // if (verbose) clog << "Opened " << altAddr.addr << ":" <<
    // myAddr.port << " --> " << info.altIpFd << endl;;
  }

  info->altIpPortFd = INVALID_SOCKET;
  if (altAddr->addr != 0)
  {
    if ((info->altIpPortFd =
         openPort (altAddr->port, altAddr->addr)) == INVALID_SOCKET)
    {
      EXEC_ERROR("Can't open %s:%u",
             DBG_stunConvertIPAddress (altAddr->addr), altAddr->port);
      stunStopServer (info);
      return false;
    }
    // if (verbose) clog << "Opened " << altAddr.addr << ":" <<
    // altAddr.port << " --> " << info.altIpPortFd << endl;;
  }

  return true;
}

void
stunStopServer (StunServerInfo * info)
{
  if (info->myFd > 0)
    close (info->myFd);
  if (info->altPortFd > 0)
    close (info->altPortFd);
  if (info->altIpFd > 0)
    close (info->altIpFd);
  if (info->altIpPortFd > 0)
    close (info->altIpPortFd);

  if (info->relay)
  {
    int i = 0;
    for (i = 0; i < MAX_MEDIA_RELAYS; ++i)
    {
      StunMediaRelay *relay = &info->relays[i];
      if (relay->fd)
      {
        close (relay->fd);
        relay->fd = 0;
      }
    }
  }
}


bool
    stunServerProcess (StunServerInfo * info)
{
  char msg[STUN_MAX_MESSAGE_SIZE];
  int msgLen = sizeof (msg);
  int i = 0;
  bool ok = false;
  bool recvAltIp = false;
  bool recvAltPort = false;
  fd_set fdSet;
  Socket maxFd = 0;

  FD_ZERO (&fdSet);
  FD_SET (info->myFd, &fdSet);

  if (info->myFd >= maxFd)
    maxFd = info->myFd + 1;

  FD_SET (info->altPortFd, &fdSet);

  if (info->altPortFd >= maxFd)
    maxFd = info->altPortFd + 1;

  if (info->altIpFd != INVALID_SOCKET)
  {
    FD_SET (info->altIpFd, &fdSet);
    if (info->altIpFd >= maxFd)
      maxFd = info->altIpFd + 1;
  }
  if (info->altIpPortFd != INVALID_SOCKET)
  {
    FD_SET (info->altIpPortFd, &fdSet);
    if (info->altIpPortFd >= maxFd)
      maxFd = info->altIpPortFd + 1;
  }

  if (info->relay)
  {
    for (i = 0; i < MAX_MEDIA_RELAYS; ++i)
    {
      StunMediaRelay *relay = &info->relays[i];
      if (relay->fd)
      {
        FD_SET (relay->fd, &fdSet);
        if (relay->fd >= maxFd)
        {
          maxFd = relay->fd + 1;
        }
      }
    }
  }

  if (info->altIpFd != INVALID_SOCKET)
  {
    FD_SET (info->altIpFd, &fdSet);
    if (info->altIpFd >= maxFd)
      maxFd = info->altIpFd + 1;
  }
  if (info->altIpPortFd != INVALID_SOCKET)
  {
    FD_SET (info->altIpPortFd, &fdSet);
    if (info->altIpPortFd >= maxFd)
      maxFd = info->altIpPortFd + 1;
  }

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 1000;

  int e = select (maxFd, &fdSet, NULL, NULL, &tv);
  if (e < 0)
  {
    EXEC_ERROR("Error on select: %s", strerror (errno));
  }
  else if (e >= 0)
  {
    StunAddress4 from;

    // do the media relaying
    if (info->relay)
    {
      time_t now = time (0);
      for (i = 0; i < MAX_MEDIA_RELAYS; ++i)
      {
        StunMediaRelay *relay = &info->relays[i];
        if (relay->fd)
        {
          if (FD_ISSET (relay->fd, &fdSet))
          {
            char msg[MAX_RTP_MSG_SIZE];
            int msgLen = sizeof (msg);

            StunAddress4 rtpFrom;
            ok = getMessage (relay->fd, msg, &msgLen,
                             &rtpFrom.addr, &rtpFrom.port);
            if (ok)
            {
              sendMessage (info->myFd, msg, msgLen,
                           relay->destination.addr, relay->destination.port);
              relay->expireTime = now + MEDIA_RELAY_TIMEOUT;

              DBG ("Relay packet on %d from %s:%u -> %x:%u",
                   relay->fd,
                   DBG_stunConvertIPAddress (rtpFrom.addr),
                   rtpFrom.port, relay->destination.addr,
                   relay->destination.port);
            }
          }
          else if (now > relay->expireTime)
          {
            close (relay->fd);
            relay->fd = 0;
          }
        }
      }
    }


    if (FD_ISSET (info->myFd, &fdSet))
    {
      DBG ("received on A1:P1");
      recvAltIp = false;
      recvAltPort = false;
      ok = getMessage (info->myFd, msg, &msgLen, &from.addr, &from.port);
    }
    else if (FD_ISSET (info->altPortFd, &fdSet))
    {
      DBG ("received on A1:P2");
      recvAltIp = false;
      recvAltPort = true;
      ok = getMessage (info->altPortFd, msg, &msgLen, &from.addr, &from.port);
    }
    else if ((info->altIpFd != INVALID_SOCKET)
             && FD_ISSET (info->altIpFd, &fdSet))
    {
      DBG ("received on A2:P1");
      recvAltIp = true;
      recvAltPort = false;
      ok = getMessage (info->altIpFd, msg, &msgLen, &from.addr, &from.port);
    }
    else if ((info->altIpPortFd != INVALID_SOCKET)
             && FD_ISSET (info->altIpPortFd, &fdSet))
    {
      DBG ("received on A2:P2");
      recvAltIp = true;
      recvAltPort = true;
      ok = getMessage (info->altIpPortFd, msg, &msgLen, &from.addr,
                       &from.port);
    }
    else
    {
      return true;
    }

    int relayPort = 0;
    if (info->relay)
    {
      for (i = 0; i < MAX_MEDIA_RELAYS; ++i)
      {
        StunMediaRelay *relay = &info->relays[i];
        if (relay->destination.addr == from.addr &&
            relay->destination.port == from.port)
        {
          relayPort = relay->relayPort;
          relay->expireTime = time (0) + MEDIA_RELAY_TIMEOUT;
          break;
        }
      }

      if (relayPort == 0)
      {
        for (i = 0; i < MAX_MEDIA_RELAYS; ++i)
        {
          StunMediaRelay *relay = &info->relays[i];
          if (relay->fd == 0)
          {
            DBG ("Open relay port %d", relay->relayPort);

            relay->fd = openPort (relay->relayPort, info->myAddr.addr);
            relay->destination.addr = from.addr;
            relay->destination.port = from.port;
            relay->expireTime = time (0) + MEDIA_RELAY_TIMEOUT;
            relayPort = relay->relayPort;
            break;
          }
        }
      }
    }

    if (!ok)
    {
      DBG ("Get message did not return a valid message");
      return true;
    }


    DBG ("Got a request (len= %d) from %s:%u", msgLen,
         DBG_stunConvertIPAddress (from.addr), from.port);


    if (msgLen <= 0)
    {
      return true;
    }

    bool changePort = false;
    bool changeIp = false;

    StunMessage resp;
    //StunAddress4 dest;
    StunAtrString hmacPassword;
    hmacPassword.sizeValue = 0;

    StunAddress4 secondary;
    secondary.port = 0;
    secondary.addr = 0;

    if (info->relay && relayPort)
    {
      secondary = from;

      from.addr = info->myAddr.addr;
      from.port = relayPort;
    }

    ok = stunServerProcessMsg (msg, msgLen, &from, &secondary,
                               recvAltIp ? &info->altAddr : &info->
                               myAddr,
                               recvAltIp ? &info->myAddr : &info->
                               altAddr, &resp, &(info->dest), &hmacPassword,
                               &changePort, &changeIp, info->natTimeSession);

    if (!ok)
    {
      DBG ("Failed to parse message");
      return true;
    }

    char buf[STUN_MAX_MESSAGE_SIZE];
    int len = sizeof (buf);

    len = stunEncodeMessage (&resp, buf, len, &hmacPassword);

    if (info->dest.addr == 0)
      ok = false;
    if (info->dest.port == 0)
      ok = false;

    if (ok)
    {
      assert (info->dest.addr != 0);
      assert (info->dest.port != 0);

      Socket sendFd;

      bool sendAltIp = recvAltIp;       // send on the
      // received IP
      // address 
      bool sendAltPort = recvAltPort;   // send on the
      // received port

      if (changeIp)
        sendAltIp = !sendAltIp; // if need to change IP, then flip 
      // logic 
      if (changePort)
        sendAltPort = !sendAltPort;     // if need to change port, 
      // then flip logic 

      if (!sendAltPort)
      {
        if (!sendAltIp)
        {
          sendFd = info->myFd;
        }
        else
        {
          sendFd = info->altIpFd;
        }
      }
      else
      {
        if (!sendAltIp)
        {
          sendFd = info->altPortFd;
        }
        else
        {
          sendFd = info->altIpPortFd;
        }
      }

      if (sendFd != INVALID_SOCKET)
      {
        sendMessage (sendFd, buf, len, info->dest.addr, info->dest.port);
      }
    }
  }

  return true;
}

int
stunFindLocalInterfaces (UInt32 * addresses, int maxRet)
{
#if defined(WIN32) || defined(__sparc__)
  return 0;
#else
  struct ifconf ifc;

  int s = socket (AF_INET, SOCK_DGRAM, 0);
  int len = 100 * sizeof (struct ifreq);

  char buf[len];

  ifc.ifc_len = len;
  ifc.ifc_buf = buf;

  int e = ioctl (s, SIOCGIFCONF, &ifc);
  char *ptr = buf;
  int tl = ifc.ifc_len;
  int count = 0;

  while ((tl > 0) && (count < maxRet))
  {
    struct ifreq *ifr = (struct ifreq *) ptr;

    int si = sizeof (ifr->ifr_name) + sizeof (struct sockaddr);
    tl -= si;
    ptr += si;
    // char* name = ifr->ifr_ifrn.ifrn_name;
    // cerr << "name = " << name << endl;

    struct ifreq ifr2;
    ifr2 = *ifr;

    e = ioctl (s, SIOCGIFADDR, &ifr2);
    if (e == -1)
    {
      break;
    }
    // cerr << "ioctl addr e = " << e << endl;

    struct sockaddr a = ifr2.ifr_addr;
    struct sockaddr_in *addr = (struct sockaddr_in *) &a;

    UInt32 ai = ntohl (addr->sin_addr.s_addr);
    if ((int) ((ai >> 24) & 0xFF) != 127)
    {
      addresses[count++] = ai;
    }
#if 0
    EXEC_ERROR("Detected interface %d.%d.%d.%d",
           int ((ai >> 24) & 0xFF),
           int ((ai >> 16) & 0xFF),
           int ((ai >> 8) & 0xFF), int ((ai) & 0xFF));
#endif
  }

  close (s);

  return count;
#endif
}


void
stunBuildReqSimple (StunMessage * msg,
                    const StunAtrString * username,
                    bool changePort, bool changeIp, unsigned int id)
{
  int i = 0;
  assert (msg);
  memset (msg, 0, sizeof (*msg));

  msg->msgHdr.msgType = BindRequestMsg;

  for (i = 0; i < 16; i = i + 4)
  {
    assert (i + 3 < 16);
    int r = stunRand ();
    msg->msgHdr.id.octet[i + 0] = r >> 0;
    msg->msgHdr.id.octet[i + 1] = r >> 8;
    msg->msgHdr.id.octet[i + 2] = r >> 16;
    msg->msgHdr.id.octet[i + 3] = r >> 24;
  }

  if (id != 0)
  {
    msg->msgHdr.id.octet[0] = id;
  }

  msg->hasChangeRequest = true;
  msg->changeRequest.value = (changeIp ? ChangeIpFlag : 0) |
    (changePort ? ChangePortFlag : 0);

  if (username->sizeValue > 0)
  {
    msg->hasUsername = true;
    msg->username = *username;
  }
}


static void
stunSendTest (Socket myFd, StunAddress4 * dest,
              const StunAtrString * username,
              const StunAtrString * password, int testNum)
{
  DBG ("-> stunSendTest");
  assert (dest->addr != 0);
  assert (dest->port != 0);

  bool changePort = false;
  bool changeIP = false;
  bool discard = false;

  switch (testNum)
  {
  case 1:
  case 10:
  case 11:
    break;
  case 2:
    // changePort=true;
    changeIP = true;
    break;
  case 3:
    changePort = true;
    break;
  case 4:
    changeIP = true;
    break;
  case 5:
    discard = true;
    break;
  default:
    EXEC_ERROR("Test %d is unkown", testNum);
    assert (0);
  }

  StunMessage req;
  memset (&req, 0, sizeof (StunMessage));

  stunBuildReqSimple (&req, username, changePort, changeIP, testNum);

  char buf[STUN_MAX_MESSAGE_SIZE];
  int len = STUN_MAX_MESSAGE_SIZE;

  len = stunEncodeMessage (&req, buf, len, password);

  DBG ("About to send msg of len %d to %s:%u", len,
       DBG_stunConvertIPAddress (dest->addr), dest->port);


  sendMessage (myFd, buf, len, dest->addr, dest->port);

  // add some delay so the packets don't get sent too quickly 
#ifdef WIN32                    // !cj! TODO - should fix this up in
  // windows
  clock_t now = clock ();
  assert (CLOCKS_PER_SEC == 1000);
  while (clock () <= now + 10)
  {
  };
#else
  usleep (10 * 1000);
#endif
  DBG ("<- stunSendTest");
}


void
stunGetUserNameAndPassword (const StunAddress4 * dest,
                            StunAtrString * username,
                            StunAtrString * password)
{
  // !cj! This is totally bogus - need to make TLS connection to dest
  // and get a
  // username and password to use 
  stunCreateUserName (dest, username);
  stunCreatePassword (username, password);
}


void
stunTest (StunAddress4 * dest, int testNum, StunAddress4 * sAddr)
{
  assert (dest->addr != 0);
  assert (dest->port != 0);

  int port = stunRandomPort ();
  UInt32 interfaceIp = 0;
  if (sAddr)
  {
    interfaceIp = sAddr->addr;
    if (sAddr->port != 0)
    {
      port = sAddr->port;
    }
  }
  Socket myFd = openPort (port, interfaceIp);

  StunAtrString username;
  StunAtrString password;

  username.sizeValue = 0;
  password.sizeValue = 0;

#ifdef USE_TLS
  stunGetUserNameAndPassword (dest, username, password);
#endif
  DBG ("stunTest");
  stunSendTest (myFd, dest, &username, &password, testNum);

  char msg[STUN_MAX_MESSAGE_SIZE];
  int msgLen = STUN_MAX_MESSAGE_SIZE;

  StunAddress4 from;
  getMessage (myFd, msg, &msgLen, &from.addr, &from.port);

  StunMessage resp;
  //bool ok = 
      stunParseMessage (msg, msgLen, &resp);

  DBG ("Got a response");

  //DBG ("ok= %d", ok);
  // clog << "\t id= " << resp.msgHdr.id << endl;

  DBG ("mappedAddr= %s:%u",
       DBG_stunConvertIPAddress (resp.mappedAddress.ipv4.addr),
       resp.mappedAddress.ipv4.port);
  DBG ("changedAddr= %s:%u",
       DBG_stunConvertIPAddress (resp.changedAddress.ipv4.addr),
       resp.changedAddress.ipv4.port);

  if (sAddr)
  {
    sAddr->port = resp.mappedAddress.ipv4.port;
    sAddr->addr = resp.mappedAddress.ipv4.addr;
  }
}

NatType
stunNatType (StunAddress4 * dest, bool * preservePort,  // if set, is
             // return for if
             // NAT preservers
             // ports or not
             bool * hairpin,    // if set, is the return for if NAT will
             // hairpin packets
             int port,          // port to use for the test, 0 to choose
             // random port
             StunAddress4 * sAddr       // NIC to use 
  )
{
  DBG ("stunNatType");

  DBG ("dest->addr first = %s", DBG_stunConvertIPAddress (dest->addr));

  assert (dest->addr != 0);
  assert (dest->port != 0);

  if (hairpin)
  {
    *hairpin = false;
  }

  if (port == 0)
  {
    port = stunRandomPort ();
  }
  UInt32 interfaceIp = 0;
  if (sAddr)
  {
    interfaceIp = sAddr->addr;
  }
  DBG ("interface IP = %d", interfaceIp);
  Socket myFd1 = openPort (port, interfaceIp);
  Socket myFd2 = openPort (port + 1, interfaceIp);

  if ((myFd1 == INVALID_SOCKET) || (myFd2 == INVALID_SOCKET))
  {
    EXEC_ERROR("Some problem opening port/interface to send on");
    return StunTypeFailure;
  }

  assert (myFd1 != INVALID_SOCKET);
  assert (myFd2 != INVALID_SOCKET);

  bool respTestI = false;
  bool isNat = true;
  StunAddress4 testIchangedAddr;
  StunAddress4 testImappedAddr;
  bool respTestI2 = false;
  bool mappedIpSame = true;
  StunAddress4 testI2mappedAddr;
  StunAddress4 testI2dest;
  bool respTestII = false;
  bool respTestIII = false;
  testI2dest.port = dest->port;
  testI2dest.addr = dest->addr;

  bool respTestHairpin = false;
  bool respTestPreservePort = false;

  memset (&testImappedAddr, 0, sizeof (testImappedAddr));

  StunAtrString username;
  StunAtrString password;

  username.sizeValue = 0;
  password.sizeValue = 0;

  /*
   * #ifdef USE_TLS stunGetUserNameAndPassword( dest, username,
   * password ); #endif
   */

  int count = 0;
  while (count < 7)
  {
    struct timeval tv;
    fd_set fdSet;
    /*
     * #ifdef WIN32 unsigned int fdSetSize; #else
     */
    int fdSetSize;
    /*
     * #endif
     */
    FD_ZERO (&fdSet);
    fdSetSize = 0;
    FD_SET (myFd1, &fdSet);
    fdSetSize = (myFd1 + 1 > fdSetSize) ? myFd1 + 1 : fdSetSize;
    FD_SET (myFd2, &fdSet);
    fdSetSize = (myFd2 + 1 > fdSetSize) ? myFd2 + 1 : fdSetSize;

      /*************************************/
    DBG ("fdSetSize = %d", fdSetSize);
      /*****************************************/
    tv.tv_sec = 0;
    tv.tv_usec = 150 * 1000;    // 150 ms 
    if (count == 0)
      tv.tv_usec = 0;

    int err = select (fdSetSize, &fdSet, NULL, NULL, &tv);
    if (err == SOCKET_ERROR)
    {
      // error occured
      EXEC_ERROR("Error %d %s in select", errno, strerror (errno));
      return StunTypeFailure;
    }
    else if (err == 0)
    {
      // timeout occured 
      count++;
      DBG ("dest->addr = %s", DBG_stunConvertIPAddress (dest->addr));

      if (!respTestI)
      {
        DBG ("Stun NAT Type respTestI");

        stunSendTest (myFd1, dest, &username, &password, 1);
      }

      if ((!respTestI2) && respTestI)
      {
        // check the address to send to if valid 
        if ((testI2dest.addr != 0) && (testI2dest.port != 0))
        {
          DBG ("Stun NAT Type respTestI2");
          stunSendTest (myFd1, &testI2dest, &username, &password, 10);
        }
      }

      if (!respTestII)
      {
        DBG ("Stun NAT Type respTestII");
        stunSendTest (myFd2, dest, &username, &password, 2);
      }

      if (!respTestIII)
      {
        DBG ("Stun NAT Type respTestIII");
        stunSendTest (myFd2, dest, &username, &password, 3);
      }

      if (respTestI && (!respTestHairpin))
      {
        if ((testImappedAddr.addr != 0) && (testImappedAddr.port != 0))
        {
          DBG ("Stun NAT Type respTestHairPin");
          stunSendTest (myFd1, &testImappedAddr, &username, &password, 11);
        }
      }

      DBG ("dest->addr 2 = %s", DBG_stunConvertIPAddress (dest->addr));

    }
    else
    {
      // if (verbose) clog <<
      // "-----------------------------------------" << endl;
      assert (err > 0);
      // data is avialbe on some fd 
      int i = 0;
      for (i = 0; i < 2; i++)
      {
        Socket myFd;
        if (i == 0)
        {
          myFd = myFd1;
        }
        else
        {
          myFd = myFd2;
        }

        if (myFd != INVALID_SOCKET)
        {
          if (FD_ISSET (myFd, &fdSet))
          {
            char msg[STUN_MAX_MESSAGE_SIZE];
            int msgLen = sizeof (msg);

            StunAddress4 from;

            getMessage (myFd, msg, &msgLen, &from.addr, &from.port);

            StunMessage resp;

            memset (&resp, 0, sizeof (StunMessage));
            memset (&resp.msgHdr, 0, sizeof (StunMsgHdr));

            stunParseMessage (msg, msgLen, &resp);


            DBG ("Received message of type %u id = %d",
                 resp.msgHdr.msgType, (int) (resp.msgHdr.id.octet[0]));


            switch (resp.msgHdr.id.octet[0])
            {
            case 1:
              {
                if (!respTestI)
                {

                  testIchangedAddr.addr = resp.changedAddress.ipv4.addr;
                  testIchangedAddr.port = resp.changedAddress.ipv4.port;
                  testImappedAddr.addr = resp.mappedAddress.ipv4.addr;
                  testImappedAddr.port = resp.mappedAddress.ipv4.port;

                  respTestPreservePort = (testImappedAddr.port == port);
                  if (preservePort)
                  {
                    *preservePort = respTestPreservePort;
                  }

                  testI2dest.addr = resp.changedAddress.ipv4.addr;

                  if (sAddr)
                  {
                    sAddr->port = testImappedAddr.port;
                    sAddr->addr = testImappedAddr.addr;
                  }

                  count = 0;
                }
                respTestI = true;
              }
              break;
            case 2:
              {
                respTestII = true;
              }
              break;
            case 3:
              {
                respTestIII = true;
              }
              break;
            case 10:
              {
                if (!respTestI2)
                {
                  testI2mappedAddr.addr = resp.mappedAddress.ipv4.addr;
                  testI2mappedAddr.port = resp.mappedAddress.ipv4.port;

                  mappedIpSame = false;
                  if ((testI2mappedAddr.addr ==
                       testImappedAddr.addr)
                      && (testI2mappedAddr.port == testImappedAddr.port))
                  {
                    mappedIpSame = true;
                  }


                }
                respTestI2 = true;
              }
              break;
            case 11:
              {

                if (hairpin)
                {
                  *hairpin = true;
                }
                respTestHairpin = true;
              }
              break;
            }
          }
        }
      }
    }
  }

  // see if we can bind to this address 
  // cerr << "try binding to " << testImappedAddr << endl;
  Socket s = openPort (0 /* use ephemeral */ , testImappedAddr.addr);
  if (s != INVALID_SOCKET)
  {
    close (s);
    isNat = false;
    // cerr << "binding worked" << endl;
  }
  else
  {
    isNat = true;
    // cerr << "binding failed" << endl;
  }


  DBG ("test I = %d", respTestI);
  DBG ("test II = %d", respTestII);
  DBG ("test III = %d", respTestIII);
  DBG ("test I(2) = %d", respTestI2);
  DBG ("is nat  = %d", isNat);
  DBG ("mapped IP same = %d", mappedIpSame);
  DBG ("hairpin = %d", respTestHairpin);
  DBG ("preserver port = %d", respTestPreservePort);

#if 0
  // implement logic flow chart from draft RFC
  if (respTestI)
  {
    if (isNat)
    {
      if (respTestII)
      {
        return StunTypeConeNat;
      }
      else
      {
        if (mappedIpSame)
        {
          if (respTestIII)
          {
            return StunTypeRestrictedNat;
          }
          else
          {
            return StunTypePortRestrictedNat;
          }
        }
        else
        {
          return StunTypeSymNat;
        }
      }
    }
    else
    {
      if (respTestII)
      {
        return StunTypeOpen;
      }
      else
      {
        return StunTypeSymFirewall;
      }
    }
  }
  else
  {
    return StunTypeBlocked;
  }
#else
  if (respTestI)                // not blocked 
  {
    if (isNat)
    {
      if (mappedIpSame)
      {
        if (respTestII)
        {
          return StunTypeIndependentFilter;
        }
        else
        {
          if (respTestIII)
          {
            return StunTypeDependentFilter;
          }
          else
          {
            return StunTypePortDependedFilter;
          }
        }
      }
      else                      // mappedIp is not same 
      {
        return StunTypeDependentMapping;
      }
    }
    else                        // isNat is false
    {
      if (respTestII)
      {
        return StunTypeOpen;
      }
      else
      {
        return StunTypeFirewall;
      }
    }
  }
  else
  {
    return StunTypeBlocked;
  }
#endif

  return StunTypeUnknown;
}


int
stunOpenSocket (StunAddress4 * dest, StunAddress4 * mapAddr,
                int port, StunAddress4 * srcAddr)
{
  assert (dest->addr != 0);
  assert (dest->port != 0);
  assert (mapAddr);

  if (port == 0)
  {
    port = stunRandomPort ();
  }
  unsigned int interfaceIp = 0;
  if (srcAddr)
  {
    interfaceIp = srcAddr->addr;
  }

  Socket myFd = openPort (port, interfaceIp);
  if (myFd == INVALID_SOCKET)
  {
    return myFd;
  }

  char msg[STUN_MAX_MESSAGE_SIZE];
  int msgLen = sizeof (msg);

  StunAtrString username;
  StunAtrString password;

  username.sizeValue = 0;
  password.sizeValue = 0;

#ifdef USE_TLS
  stunGetUserNameAndPassword (dest, username, password);
#endif

  stunSendTest (myFd, dest, &username, &password, 1);

  StunAddress4 from;

  getMessage (myFd, msg, &msgLen, &from.addr, &from.port);

  StunMessage resp;
  memset (&resp, 0, sizeof (StunMessage));

  bool ok = stunParseMessage (msg, msgLen, &resp);
  if (!ok)
  {
    return -1;
  }

  StunAddress4 mappedAddr = resp.mappedAddress.ipv4;
  // StunAddress4 changedAddr = resp.changedAddress.ipv4; //NOT USED

  // clog << "--- stunOpenSocket --- " << endl;
  // clog << "\treq id=" << req.id << endl;
  // clog << "\tresp id=" << id << endl;
  // clog << "\tmappedAddr=" << mappedAddr << endl;

  *mapAddr = mappedAddr;

  return myFd;
}


bool
stunOpenSocketPair (StunAddress4 * dest, StunAddress4 * mapAddr,
                    int *fd1, int *fd2, int port, StunAddress4 * srcAddr)
{
  assert (dest->addr != 0);
  assert (dest->port != 0);
  assert (mapAddr);

  const int NUM = 3;

  if (port == 0)
  {
    port = stunRandomPort ();
  }

  *fd1 = -1;
  *fd2 = -1;

  char msg[STUN_MAX_MESSAGE_SIZE];
  int msgLen = sizeof (msg);

  StunAddress4 from;
  int fd[NUM];
  int i;

  unsigned int interfaceIp = 0;
  if (srcAddr)
  {
    interfaceIp = srcAddr->addr;
  }

  for (i = 0; i < NUM; i++)
  {
    fd[i] = openPort ((port == 0) ? 0 : (port + i), interfaceIp);
    if (fd[i] < 0)
    {
      while (i > 0)
      {
        close (fd[--i]);
      }
      return false;
    }
  }

  StunAtrString username;
  StunAtrString password;

  username.sizeValue = 0;
  password.sizeValue = 0;

#ifdef USE_TLS
  stunGetUserNameAndPassword (dest, username, password);
#endif

  for (i = 0; i < NUM; i++)
  {
    stunSendTest (fd[i], dest, &username, &password, 1  /* testNum 
                                                         */ );
  }

  StunAddress4 mappedAddr[NUM];
  for (i = 0; i < NUM; i++)
  {
    msgLen = sizeof (msg) / sizeof (*msg);
    getMessage (fd[i], msg, &msgLen, &from.addr, &from.port);

    StunMessage resp;
    memset (&resp, 0, sizeof (StunMessage));

    bool ok = stunParseMessage (msg, msgLen, &resp);
    if (!ok)
    {
      return false;
    }

    mappedAddr[i] = resp.mappedAddress.ipv4;
    // StunAddress4 changedAddr = resp.changedAddress.ipv4; not used
  }


  DBG ("--- stunOpenSocketPair ---");
  for (i = 0; i < NUM; i++)
  {

    DBG ("mappedAddr= %s:%u",
         DBG_stunConvertIPAddress (mappedAddr[i].addr), mappedAddr[i].port);
  }


  if (mappedAddr[0].port % 2 == 0)
  {
    if (mappedAddr[0].port + 1 == mappedAddr[1].port)
    {
      *mapAddr = mappedAddr[0];
      *fd1 = fd[0];
      *fd2 = fd[1];
      close (fd[2]);
      return true;
    }
  }
  else
  {
    if ((mappedAddr[1].port % 2 == 0)
        && (mappedAddr[1].port + 1 == mappedAddr[2].port))
    {
      *mapAddr = mappedAddr[1];
      *fd1 = fd[1];
      *fd2 = fd[2];
      close (fd[0]);
      return true;
    }
  }

  // something failed, close all and return error
  for (i = 0; i < NUM; i++)
  {
    close (fd[i]);
  }

  return false;
}

/*
 * ====================================================================
 * The Vovida Software License, Version 1.0 Copyright (c) 2000 Vovida
 * Networks, Inc.  All rights reserved. Redistribution and use in source
 * and binary forms, with or without modification, are permitted provided
 * that the following conditions are met: 1. Redistributions of source
 * code must retain the above copyright notice, this list of conditions
 * and the following disclaimer. 2. Redistributions in binary form must
 * reproduce the above copyright notice, this list of conditions and the
 * following disclaimer in the documentation and/or other materials
 * provided with the distribution. 3. The names "VOCAL", "Vovida Open
 * Communication Application Library", and "Vovida Open Communication
 * Application Library (VOCAL)" must not be used to endorse or promote
 * products derived from this software without prior written permission.
 * For written permission, please contact vocal@vovida.org. 4. Products
 * derived from this software may not be called "VOCAL", nor may "VOCAL"
 * appear in their name, without prior written permission of Vovida
 * Networks, Inc. THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE
 * AND NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES IN
 * EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks, Inc.
 * For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>. 
 */


/* 
 * Contributions: 25/01/2009
 * Author       : Orange
 * Update the udp and stun source code for the static stun library
 * and adaptation of the trace system
 */


// Local Variables:
// mode:c++
// c-file-style:"ellemtel"
// c-file-offsets:((case-label . +))
// indent-tabs-mode:nil
// End:
