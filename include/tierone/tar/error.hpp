/*
 * Copyright 2025 TierOne Software
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <expected>
#include <string>
#include <system_error>

namespace tierone::tar {

enum class error_code {
    invalid_header,
    corrupt_archive,
    io_error,
    unsupported_feature,
    invalid_operation,
    end_of_archive
};

class error {
public:
    error(const error_code code, std::string message)
        : code_(code), message_(std::move(message)) {}

    [[nodiscard]] error_code code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

private:
    error_code code_;
    std::string message_;
};

} // namespace tierone::tar