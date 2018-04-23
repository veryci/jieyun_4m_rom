/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: uadata.proto */

#ifndef PROTOBUF_C_uadata_2eproto__INCLUDED
#define PROTOBUF_C_uadata_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1000000 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _IosWorkQuery IosWorkQuery;
typedef struct _UaItem UaItem;
typedef struct _IosWorkItem IosWorkItem;


/* --- enums --- */


/* --- messages --- */

struct  _IosWorkQuery
{
  ProtobufCMessage base;
  char *userid;
  char *url;
  char *headers;
  char *clientmac;
  char *channel;
};
#define IOS_WORK_QUERY__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&ios_work_query__descriptor) \
    , NULL, NULL, NULL, NULL, NULL }


struct  _UaItem
{
  ProtobufCMessage base;
  size_t n_headers;
  char **headers;
};
#define UA_ITEM__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&ua_item__descriptor) \
    , 0,NULL }


struct  _IosWorkItem
{
  ProtobufCMessage base;
  char *workurl;
  char *guid;
  size_t n_ualist;
  UaItem **ualist;
};
#define IOS_WORK_ITEM__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&ios_work_item__descriptor) \
    , NULL, NULL, 0,NULL }


/* IosWorkQuery methods */
void   ios_work_query__init
                     (IosWorkQuery         *message);
size_t ios_work_query__get_packed_size
                     (const IosWorkQuery   *message);
size_t ios_work_query__pack
                     (const IosWorkQuery   *message,
                      uint8_t             *out);
size_t ios_work_query__pack_to_buffer
                     (const IosWorkQuery   *message,
                      ProtobufCBuffer     *buffer);
IosWorkQuery *
       ios_work_query__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   ios_work_query__free_unpacked
                     (IosWorkQuery *message,
                      ProtobufCAllocator *allocator);
/* UaItem methods */
void   ua_item__init
                     (UaItem         *message);
size_t ua_item__get_packed_size
                     (const UaItem   *message);
size_t ua_item__pack
                     (const UaItem   *message,
                      uint8_t             *out);
size_t ua_item__pack_to_buffer
                     (const UaItem   *message,
                      ProtobufCBuffer     *buffer);
UaItem *
       ua_item__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   ua_item__free_unpacked
                     (UaItem *message,
                      ProtobufCAllocator *allocator);
/* IosWorkItem methods */
void   ios_work_item__init
                     (IosWorkItem         *message);
size_t ios_work_item__get_packed_size
                     (const IosWorkItem   *message);
size_t ios_work_item__pack
                     (const IosWorkItem   *message,
                      uint8_t             *out);
size_t ios_work_item__pack_to_buffer
                     (const IosWorkItem   *message,
                      ProtobufCBuffer     *buffer);
IosWorkItem *
       ios_work_item__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   ios_work_item__free_unpacked
                     (IosWorkItem *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*IosWorkQuery_Closure)
                 (const IosWorkQuery *message,
                  void *closure_data);
typedef void (*UaItem_Closure)
                 (const UaItem *message,
                  void *closure_data);
typedef void (*IosWorkItem_Closure)
                 (const IosWorkItem *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor ios_work_query__descriptor;
extern const ProtobufCMessageDescriptor ua_item__descriptor;
extern const ProtobufCMessageDescriptor ios_work_item__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_uadata_2eproto__INCLUDED */
