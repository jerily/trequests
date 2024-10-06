# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

# https://github.com/openai/openai-openapi/blob/master/openapi.yaml

package require trequests
package require tjson

set api_url "https://api.openai.com/v1"
set api_key $env(OPENAI_API_KEY)

set url "$api_url/chat/completions"
set headers [dict create \
    "Authorization" "Bearer $api_key"
]



set json [::tjson::typed_to_json [list M [list \
    "model" [list S "gpt-3.5-turbo"] \
    "messages" [list L [list \
        [list M [list \
            "role" [list S "system"] \
            "content" [list S "You are a helpful assistant."] \
        ]]\
        [list M [list \
            "role" [list S "user"] \
            "content" [list S "What is the purpose of life?"] \
        ]]\
    ]] \
]]]

set r [trequests::post $url -headers $headers -json $json]

puts response=[$r text]
::tjson::parse [$r text] json_handle
set message_handle [::tjson::query $json_handle {$.choices[0].message.content}]
set message [::tjson::get_valuestring $message_handle]
puts "message=$message"

catch {$r destroy}
