; Sticky context lines for nvim-treesitter-context.

(conditional
  consequence: (_) @context.end) @context

[
  (object)
  (object_comp)
  (array)
  (array_comp)
  (function)
  (call)
  (local)
  (binding)
  (field)
  (for_spec)
] @context
