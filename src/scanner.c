#include "tree_sitter/alloc.h"
#include "tree_sitter/array.h"
#include "tree_sitter/parser.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum TokenType
{
    TEXT_BLOCK_START,
    TEXT_BLOCK_CONTENT,
    TEXT_BLOCK_END,
} TokenType;

typedef Array(char) CharArray;

void *tree_sitter_jsonnet_external_scanner_create()
{
    // An array storing the initial indentation of the text block.
    return ts_calloc(1, sizeof(CharArray));
}

void tree_sitter_jsonnet_external_scanner_destroy(void *payload)
{
    CharArray *indent_chars = (CharArray *)payload;
    array_delete(indent_chars);
    ts_free(payload);
}

unsigned tree_sitter_jsonnet_external_scanner_serialize(void *payload, char *buffer)
{
    return 0;
}

void tree_sitter_jsonnet_external_scanner_deserialize(void *payload, const char *buffer, unsigned length)
{
}

/** Checks whether the lexer has reached the EOF.*/
inline static bool eof(TSLexer *lexer)
{
    return lexer->eof(lexer);
}

/** Advances the cursor by one character. */
inline static void advance(TSLexer *lexer)
{
    lexer->advance(lexer, false);
}

/** Skips the next character. */
inline static void skip(TSLexer *lexer)
{
    lexer->advance(lexer, true);
}

/** Checks whether the next character matches the given character without advancing the cursor. */
inline static bool lookahead(TSLexer *lexer, uint32_t codepoint)
{
    return lexer->lookahead == codepoint;
}

/**
 * Tries to match a single character.
 *
 * When the next character matches the given one, returns `true` and advances the cursor by one character. Otherwise,
 * returns `false`.
 */
inline static bool match(TSLexer *lexer, const char ch)
{
    if (!lookahead(lexer, ch))
        return false;

    advance(lexer);
    return true;
}

/**
 * Tries to match a given string.
 *
 * When the successive input matches the given string pattern, returns `true` and advances the cursor by the length of
 * the pattern. Otherwise, returns `false`.
 */
inline static bool match_string(TSLexer *lexer, char const *string)
{
    for (char const *p = string; *p != '\0'; ++p)
        if (eof(lexer) || !match(lexer, *p))
            return false;

    return true;
}

/**
 * Tries to match any character in the given string.
 *
 * When the next character matches any character in the given string, returns `true` and advances the cursor by one
 * character. Otherwise, returns `false`.
 */
inline static bool match_any(TSLexer *lexer, char const *charset)
{
    for (char const *p = charset; !eof(lexer) && *p != '\0'; ++p)
        if (match(lexer, *p))
            return true;

    return false;
}

/**
 * Advances the cursor after the first occurrence of the given character.
 *
 * If the cursor indeed advanced after the first occurrence of the given character, returns `true`. Otherwise, returns
 * `false`, indicating the cursor has reached the EOF.
 */
inline static bool advance_after(TSLexer *lexer, const char ch)
{
    while (true)
    {
        if (eof(lexer))
            return false;

        if (lookahead(lexer, ch))
        {
            advance(lexer);
            return true;
        }

        advance(lexer);
    }
}

/**
 * Advances the cursor repeatedly when the next character is in the given character set.
 *
 * If the cursor indeed reached a character not in the character set, returns `true`. Otherwise, returns `false`,
 * indicating that the cursor has reached the EOF.
 */
inline static bool advance_while_any(TSLexer *lexer, char const *charset)
{
    while (match_any(lexer, charset))
        ;

    return !eof(lexer);
}

inline static bool match_indent(TSLexer *lexer, CharArray *indent_chars)
{
    for (uint32_t i = 0; i < indent_chars->size; ++i)
        if (!match(lexer, *array_get(indent_chars, i)))
            return false;
    return true;
}

inline static bool scan_text_block_start(void *payload, TSLexer *lexer)
{
    if (!match_string(lexer, "|||"))
        return false;
    // Scans the optional trailing dash, indicating that the last newline of the text block must be removed.
    match(lexer, '-');

    // The starting fence must end with zero or more whitespaces and a newline.
    if (!advance_while_any(lexer, " \t") || !match(lexer, '\n'))
        return false;

    lexer->mark_end(lexer);
    lexer->result_symbol = TEXT_BLOCK_START;
    return true;
}

inline static bool scan_text_block_end(void *payload, TSLexer *lexer)
{
    if (!match_string(lexer, "|||"))
        return false;

    lexer->mark_end(lexer);
    lexer->result_symbol = TEXT_BLOCK_END;
    return true;
}

static bool scan_text_block_line(void *payload, TSLexer *lexer)
{
    return false;
}

static bool scan_text_block_content(void *payload, TSLexer *lexer)
{
    // A text block can have zero or more leading empty lines.
    if (!advance_while_any(lexer, "\n"))
        // Reached
        return false;

    CharArray *indent_chars = (CharArray *)payload;
    array_clear(indent_chars);

    // Scans the first line of the text block.
    {
        // Scans the initial indentation.
        {
            while (!eof(lexer) && (lookahead(lexer, ' ') || lookahead(lexer, '\t')))
            {
                array_push(indent_chars, (char)lexer->lookahead);
                advance(lexer);
            }

            if (indent_chars->size == 0)
                // Empty indentation
                return false;
        }

        // Scans the rest of the first line.
        if (!advance_after(lexer, '\n'))
            // Reached the EOF prematurely before seeing the newline.
            return false;

        // Commit the first line as a possible content end, so a fence on the very next line is not swallowed.
        lexer->mark_end(lexer);
    }

    // Scans the rest of the text block, one line at a time.
    while (true)
    {
        // Checks the leading indentation.
        if (match_indent(lexer, indent_chars))
        {
            // Scans the rest of the line.
            if (!advance_after(lexer, '\n'))
                // Reached the EOF prematurely before seeing the newline.
                return false;
        }
        else
        {
            // The leading whitespaces do not match the initial indentation. This line could be:
            //  1. An in-block blank line (empty or whitespace-only), or
            //  2. The ending fence, or
            //  3. Invalid

            // Consume any remaining leading whitespaces on this line.
            advance_while_any(lexer, " \t");

            if (lookahead(lexer, '\n'))
                // The line only contains whitespaces and counts as an in-block blank line.
                advance(lexer);
            else if (match_string(lexer, "|||"))
            {
                // Ending fence detected, finishing a text block.
                lexer->result_symbol = TEXT_BLOCK_CONTENT;
                return true;
            }
            else
                // Anything else (including EOF before a closing fence) is invalid.
                return false;
        }

        // This line could be the last line, tentatively marks the end of the text block content token.
        lexer->mark_end(lexer);
    }
}

bool tree_sitter_jsonnet_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols)
{
    if (valid_symbols[TEXT_BLOCK_START])
        return scan_text_block_start(payload, lexer);
    else if (valid_symbols[TEXT_BLOCK_CONTENT])
        return scan_text_block_content(payload, lexer);
    else if (valid_symbols[TEXT_BLOCK_END])
        return scan_text_block_end(payload, lexer);
    else
        return false;
}
