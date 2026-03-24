#pragma once

#include <string>

namespace vse {

enum class ErrorCode {
    None = 0,
    FileNotFound,
    FileReadError,
    JsonParseError,
    InvalidArgument,
    OutOfRange,
    SystemError
};

template<typename T>
struct Result {
    T value;
    ErrorCode error;
    
    bool ok() const { return error == ErrorCode::None; }
    
    static Result<T> success(T v) {
        return Result<T>{v, ErrorCode::None};
    }
    
    static Result<T> failure(ErrorCode e) {
        return Result<T>{T(), e};
    }
};

} // namespace vse