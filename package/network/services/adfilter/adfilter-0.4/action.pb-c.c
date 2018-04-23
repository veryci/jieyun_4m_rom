/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: action.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "action.pb-c.h"
void   http_data_action__init
                     (HttpDataAction         *message)
{
  static HttpDataAction init_value = HTTP_DATA_ACTION__INIT;
  *message = init_value;
}
size_t http_data_action__get_packed_size
                     (const HttpDataAction *message)
{
  assert(message->base.descriptor == &http_data_action__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t http_data_action__pack
                     (const HttpDataAction *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &http_data_action__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t http_data_action__pack_to_buffer
                     (const HttpDataAction *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &http_data_action__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
HttpDataAction *
       http_data_action__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (HttpDataAction *)
     protobuf_c_message_unpack (&http_data_action__descriptor,
                                allocator, len, data);
}
void   http_data_action__free_unpacked
                     (HttpDataAction *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &http_data_action__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   http_redirect_action__init
                     (HttpRedirectAction         *message)
{
  static HttpRedirectAction init_value = HTTP_REDIRECT_ACTION__INIT;
  *message = init_value;
}
size_t http_redirect_action__get_packed_size
                     (const HttpRedirectAction *message)
{
  assert(message->base.descriptor == &http_redirect_action__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t http_redirect_action__pack
                     (const HttpRedirectAction *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &http_redirect_action__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t http_redirect_action__pack_to_buffer
                     (const HttpRedirectAction *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &http_redirect_action__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
HttpRedirectAction *
       http_redirect_action__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (HttpRedirectAction *)
     protobuf_c_message_unpack (&http_redirect_action__descriptor,
                                allocator, len, data);
}
void   http_redirect_action__free_unpacked
                     (HttpRedirectAction *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &http_redirect_action__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   http_action__init
                     (HttpAction         *message)
{
  static HttpAction init_value = HTTP_ACTION__INIT;
  *message = init_value;
}
size_t http_action__get_packed_size
                     (const HttpAction *message)
{
  assert(message->base.descriptor == &http_action__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t http_action__pack
                     (const HttpAction *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &http_action__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t http_action__pack_to_buffer
                     (const HttpAction *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &http_action__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
HttpAction *
       http_action__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (HttpAction *)
     protobuf_c_message_unpack (&http_action__descriptor,
                                allocator, len, data);
}
void   http_action__free_unpacked
                     (HttpAction *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &http_action__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   http_header_item__init
                     (HttpHeaderItem         *message)
{
  static HttpHeaderItem init_value = HTTP_HEADER_ITEM__INIT;
  *message = init_value;
}
size_t http_header_item__get_packed_size
                     (const HttpHeaderItem *message)
{
  assert(message->base.descriptor == &http_header_item__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t http_header_item__pack
                     (const HttpHeaderItem *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &http_header_item__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t http_header_item__pack_to_buffer
                     (const HttpHeaderItem *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &http_header_item__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
HttpHeaderItem *
       http_header_item__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (HttpHeaderItem *)
     protobuf_c_message_unpack (&http_header_item__descriptor,
                                allocator, len, data);
}
void   http_header_item__free_unpacked
                     (HttpHeaderItem *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &http_header_item__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   http_action_query__init
                     (HttpActionQuery         *message)
{
  static HttpActionQuery init_value = HTTP_ACTION_QUERY__INIT;
  *message = init_value;
}
size_t http_action_query__get_packed_size
                     (const HttpActionQuery *message)
{
  assert(message->base.descriptor == &http_action_query__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t http_action_query__pack
                     (const HttpActionQuery *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &http_action_query__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t http_action_query__pack_to_buffer
                     (const HttpActionQuery *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &http_action_query__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
HttpActionQuery *
       http_action_query__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (HttpActionQuery *)
     protobuf_c_message_unpack (&http_action_query__descriptor,
                                allocator, len, data);
}
void   http_action_query__free_unpacked
                     (HttpActionQuery *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &http_action_query__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor http_data_action__field_descriptors[1] =
{
  {
    "Data",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_BYTES,
    0,   /* quantifier_offset */
    offsetof(HttpDataAction, data),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned http_data_action__field_indices_by_name[] = {
  0,   /* field[0] = Data */
};
static const ProtobufCIntRange http_data_action__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor http_data_action__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "HttpDataAction",
  "HttpDataAction",
  "HttpDataAction",
  "",
  sizeof(HttpDataAction),
  1,
  http_data_action__field_descriptors,
  http_data_action__field_indices_by_name,
  1,  http_data_action__number_ranges,
  (ProtobufCMessageInit) http_data_action__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor http_redirect_action__field_descriptors[6] =
{
  {
    "Host",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpRedirectAction, host),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Port",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_INT32,
    0,   /* quantifier_offset */
    offsetof(HttpRedirectAction, port),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Url",
    3,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpRedirectAction, url),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Method",
    4,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpRedirectAction, method),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "HttpVer",
    5,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpRedirectAction, httpver),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Referer",
    6,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpRedirectAction, referer),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned http_redirect_action__field_indices_by_name[] = {
  0,   /* field[0] = Host */
  4,   /* field[4] = HttpVer */
  3,   /* field[3] = Method */
  1,   /* field[1] = Port */
  5,   /* field[5] = Referer */
  2,   /* field[2] = Url */
};
static const ProtobufCIntRange http_redirect_action__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 6 }
};
const ProtobufCMessageDescriptor http_redirect_action__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "HttpRedirectAction",
  "HttpRedirectAction",
  "HttpRedirectAction",
  "",
  sizeof(HttpRedirectAction),
  6,
  http_redirect_action__field_descriptors,
  http_redirect_action__field_indices_by_name,
  1,  http_redirect_action__number_ranges,
  (ProtobufCMessageInit) http_redirect_action__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor http_action__field_descriptors[3] =
{
  {
    "Action",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_ENUM,
    0,   /* quantifier_offset */
    offsetof(HttpAction, action),
    &action_flag__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Data",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(HttpAction, data),
    &http_data_action__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Redirect",
    3,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(HttpAction, redirect),
    &http_redirect_action__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned http_action__field_indices_by_name[] = {
  0,   /* field[0] = Action */
  1,   /* field[1] = Data */
  2,   /* field[2] = Redirect */
};
static const ProtobufCIntRange http_action__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 3 }
};
const ProtobufCMessageDescriptor http_action__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "HttpAction",
  "HttpAction",
  "HttpAction",
  "",
  sizeof(HttpAction),
  3,
  http_action__field_descriptors,
  http_action__field_indices_by_name,
  1,  http_action__number_ranges,
  (ProtobufCMessageInit) http_action__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor http_header_item__field_descriptors[2] =
{
  {
    "Key",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpHeaderItem, key),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Value",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpHeaderItem, value),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned http_header_item__field_indices_by_name[] = {
  0,   /* field[0] = Key */
  1,   /* field[1] = Value */
};
static const ProtobufCIntRange http_header_item__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor http_header_item__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "HttpHeaderItem",
  "HttpHeaderItem",
  "HttpHeaderItem",
  "",
  sizeof(HttpHeaderItem),
  2,
  http_header_item__field_descriptors,
  http_header_item__field_indices_by_name,
  1,  http_header_item__number_ranges,
  (ProtobufCMessageInit) http_header_item__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor http_action_query__field_descriptors[4] =
{
  {
    "UserID",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpActionQuery, userid),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Url",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpActionQuery, url),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Method",
    3,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(HttpActionQuery, method),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "Headers",
    4,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(HttpActionQuery, n_headers),
    offsetof(HttpActionQuery, headers),
    &http_header_item__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned http_action_query__field_indices_by_name[] = {
  3,   /* field[3] = Headers */
  2,   /* field[2] = Method */
  1,   /* field[1] = Url */
  0,   /* field[0] = UserID */
};
static const ProtobufCIntRange http_action_query__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 4 }
};
const ProtobufCMessageDescriptor http_action_query__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "HttpActionQuery",
  "HttpActionQuery",
  "HttpActionQuery",
  "",
  sizeof(HttpActionQuery),
  4,
  http_action_query__field_descriptors,
  http_action_query__field_indices_by_name,
  1,  http_action_query__number_ranges,
  (ProtobufCMessageInit) http_action_query__init,
  NULL,NULL,NULL    /* reserved[123] */
};
const ProtobufCEnumValue action_flag__enum_values_by_number[4] =
{
  { "ActionNone", "ACTION_FLAG__ActionNone", 1 },
  { "ActionData", "ACTION_FLAG__ActionData", 2 },
  { "ActionRedirect", "ACTION_FLAG__ActionRedirect", 3 },
  { "ActionReplaceRequest", "ACTION_FLAG__ActionReplaceRequest", 4 },
};
static const ProtobufCIntRange action_flag__value_ranges[] = {
{1, 0},{0, 4}
};
const ProtobufCEnumValueIndex action_flag__enum_values_by_name[4] =
{
  { "ActionData", 1 },
  { "ActionNone", 0 },
  { "ActionRedirect", 2 },
  { "ActionReplaceRequest", 3 },
};
const ProtobufCEnumDescriptor action_flag__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "ActionFlag",
  "ActionFlag",
  "ActionFlag",
  "",
  4,
  action_flag__enum_values_by_number,
  4,
  action_flag__enum_values_by_name,
  1,
  action_flag__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};