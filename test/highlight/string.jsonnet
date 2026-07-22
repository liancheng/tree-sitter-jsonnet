[
    "a\tb",
//  ^^ string
//    ^^ string.escape
//      ^^ string

    "a\u0041b",
//  ^^ string
//    ^^^^^^ string.escape
//          ^^ string

    "a\nb",
//  ^^ string
//    ^^ string.escape
//      ^^ string

    "a\"b",
//  ^^ string
//    ^^ string.escape
//      ^^ string

    @"""\n",
//  ^^ string
//    ^^ string.escape
//      ^^ string
//        ^ string

    @'''\n',
//  ^^ string
//    ^^ string.escape
//      ^^ string
//        ^ string

    @"a\nb",
//  ^^^ string
//     ^^ string
//       ^^ string

    @'a\nb',
//  ^^^ string
//     ^^ string
//       ^^ string

    |||
      \n
//    ^^ string
    |||
]
