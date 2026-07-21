; Literals
(number) @number
(boolean) @boolean
(null) @constant.builtin

(quoted_string) @string
(text_block) @string
(escape_sequence) @string.escape

(comment) @comment @spell

; Keywords
[
  "assert"
  "error"
  "import"
  "importstr"
  "importbin"
  "tailstrict"
] @keyword

[
  "if"
  "then"
  "else"
] @keyword.conditional

"for" @keyword.repeat
"function" @keyword.function
"local" @keyword

"in" @keyword.operator

; Special variables
[
  (self)
  (super)
  (dollar)
] @variable.builtin

; Object fields
(field key: (field_key (field_id) @property))
(field key: (field_key (string) @property))
(object_local "local" @keyword)

; Method / function definition names
(field
  key: (field_key (field_id) @function.method)
  params: (params))

; Bindings & parameters
(binding function: (var_id) @function)
(binding variable: (var_id) @variable)
(param (var_id) @variable.parameter)
(param_ref_id) @variable.parameter

; Field access
(field_access field: (field_ref_id) @property)

; Function calls
(call callee: (var_ref_id) @function.call)

; Variables (fallback)
(var_ref_id) @variable

; Operators
[
  (multiplicative)
  (additive)
  (bit_shift)
  (comparison)
  (equality)
  (bit_and)
  (bit_xor)
  (bit_or)
  (and)
  (or)
  (unary_operator)
] @operator

(field visibility: _ @operator)
(field inherit: _ @operator)

; Punctuation
["{" "}"] @punctuation.bracket
["[" "]"] @punctuation.bracket
["(" ")"] @punctuation.bracket

["," "." ";" ":"] @punctuation.delimiter
