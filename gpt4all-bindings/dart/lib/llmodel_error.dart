import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';

class LLModelError extends ffi.Struct {

  external ffi.Pointer<Utf8> message;

  @ffi.Int32()
  external int code;
}
