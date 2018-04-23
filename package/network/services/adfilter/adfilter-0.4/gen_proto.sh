#! /bin/sh

cp ~/source/go/src/filtersvr/business/business.proto ./
protoc-c --c_out=. business.proto

cp ~/source/go/src/httpsvr/actionsvr/action/action.proto ./
protoc-c --c_out=. action.proto

cp ~/source/go/src/uaanalyze/uadata/uadata.proto ./
protoc-c --c_out=. uadata.proto


