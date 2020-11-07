/*
	JSON library version 3.0.0 2020-11-07 by Santtu Nyman.
	git repository https://github.com/Santtu-Nyman/jsonl
	
	License
		This is free and unencumbered software released into the public domain.

		Anyone is free to copy, modify, publish, use, compile, sell, or
		distribute this software, either in source code form or as a compiled
		binary, for any purpose, commercial or non-commercial, and by any
		means.

		In jurisdictions that recognize copyright laws, the author or authors
		of this software dedicate any and all copyright interest in the
		software to the public domain. We make this dedication for the benefit
		of the public at large and to the detriment of our heirs and
		successors. We intend this dedication to be an overt act of
		relinquishment in perpetuity of all present and future rights to this
		software under copyright law.

		THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
		EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
		MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
		IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
		OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
		ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
		OTHER DEALINGS IN THE SOFTWARE.

		For more information, please refer to <https://unlicense.org>
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "jsonl.h"

#if defined(_DEBUG) || defined(DEBUG)
#if defined(JSONL_USE_CRT_ASSERT)
#include <assert.h>
#define JSONL_ASSERT(x) assert(x)
#else
static void jsonl_debug_assert(int x, int location)
{
	if (!x)
		*(volatile int*)0 = location;
}
#define JSONL_ASSERT(x) jsonl_debug_assert((int)(x), (int)(__LINE__))
#endif
#endif
#if defined(__GNUC__)
#if !defined(_DEBUG) && !defined(DEBUG)
#define JSONL_ASSERT(x) do { if (!(x)) __builtin_unreachable(); } while (0)
#endif
#define JSONL_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#if !defined(_DEBUG) && !defined(DEBUG)
#define JSONL_ASSERT(x) __assume(x)
#endif
#define JSONL_INLINE __forceinline
#else
#if !defined(_DEBUG) && !defined(DEBUG)
#define JSONL_ASSERT(x) do {} while (0)
#endif
#define JSONL_INLINE inline
#endif

typedef struct jsonl_internal_path_t
{
	struct jsonl_internal_path_t* parent;
	size_t depth;
	jsonl_path_component_t path_component;
	int value_type;
	const jsonl_value_t* source_value;
} jsonl_internal_path_t;

static const size_t jsonl_value_alignment_minus_one = (((sizeof(double) > sizeof(void*)) ? sizeof(double) : ((sizeof(int) > sizeof(void*)) ? sizeof(int) : sizeof(void*))) - 1);

static JSONL_INLINE size_t jsonl_round_size(size_t size) { return (size + jsonl_value_alignment_minus_one) & ~jsonl_value_alignment_minus_one; }

static JSONL_INLINE int jsonl_is_white_space(char character) { return character == ' ' || character == '\t' || character == '\n' || character == '\r'; }

static JSONL_INLINE int jsonl_is_structural_character(char character) { return character == '[' || character == '{' || character == ']' || character == '}' || character == ':' || character == ',' || character == '"'; }

static JSONL_INLINE int jsonl_is_decimal_value(char character) { return character >= '0' && character <= '9'; }

static JSONL_INLINE int jsonl_is_hex_value(char character) { return (character >= '0' && character <= '9') || (character >= 'A' && character <= 'F') || (character >= 'a' && character <= 'f'); }

static JSONL_INLINE int jsonl_decimal_value(char character) { return character - '0'; }

static JSONL_INLINE int jsonl_hex_value(char character) { return (character <= '9') ? (character - '0') : ((character <= 'F') ? (character - ('A' - 10)) : (character - ('f' - 10))); }

static JSONL_INLINE size_t jsonl_string_length(const char* string)
{
	const char* string_end = string;
	while (*string_end)
		++string_end;
	return (size_t)((uintptr_t)string_end - (uintptr_t)string);
}

static JSONL_INLINE int jsonl_memory_compare(const void* block_a, const void* block_b, size_t size)
{
	const uint8_t* read_a = (const uint8_t*)block_a;
	const uint8_t* read_b = (const uint8_t*)block_b;
	const uint8_t* a_end = (const uint8_t*)((uintptr_t)read_a + size);
	while (read_a != a_end && *read_a == *read_b)
	{
		++read_a;
		++read_b;
	}
	return read_a == a_end;
}

static JSONL_INLINE void jsonl_copy_memory(void* destination, const void* source, size_t size)
{
	uint8_t* write = (uint8_t*)destination;
	const uint8_t* read = (const uint8_t*)source;
	const uint8_t* read_end = (const uint8_t*)((uintptr_t)source + size);
	while (read != read_end)
		*write++ = *read++;
}

static size_t jsonl_white_space_length(size_t json_text_size, const char* json_text);

static size_t jsonl_printable_space_length(size_t json_text_size, const char* json_text);

static size_t jsonl_quoted_string_lenght(size_t json_text_size, const char* json_text);

static size_t jsonl_decode_boolean(size_t json_text_size, const char* json_text, int* value);

static size_t jsonl_decode_quoted_string(size_t json_text_size, const char* json_text, size_t string_buffer_size, char* string_buffer, size_t* string_size);

static size_t jsonl_decode_number(size_t json_text_size, const char* json_text, jsonl_number_value_t* number_value);

static size_t jsonl_object_text_size(size_t json_text_size, const char* json_text);

static size_t jsonl_array_text_size(size_t json_text_size, const char* json_text);

static int jsonl_decide_value_type(size_t json_text_size, const char* json_text, size_t* object_text_size);

static size_t jsonl_name_value_pair_size(size_t json_text_size, const char* json_text, size_t* name_size, int* value_type, size_t* value_offset, size_t* value_size);

static size_t jsonl_count_object_name_value_pairs(size_t json_text_size, const char* json_text);

static size_t jsonl_count_array_values(size_t json_text_size, const char* json_text);

static void jsonl_terminator_string(char* string_end);

static size_t jsonl_create_tree_from_text(size_t json_text_size, const char* json_text, size_t value_buffer_size, jsonl_value_t* value_buffer, jsonl_value_t* parent_value, size_t* required_value_buffer_size);

static size_t jsonl_print_number_value(const jsonl_value_t* number_value, size_t text_buffer_size, char* text_buffer);

static size_t jsonl_print_string_value(size_t string_size, const char* string, size_t text_buffer_size, char* text_buffer);

static void jsonl_print_indent(char* text_buffer, size_t depth);

static size_t jsonl_internal_print(const jsonl_value_t* value_tree, size_t json_text_buffer_size, char* json_text_buffer, size_t depth);

static size_t jsonl_internal_create_null_value(const jsonl_value_t* parent, size_t value_buffer_size, jsonl_value_t* value_buffer);

static size_t jsonl_internal_copy_value(const jsonl_value_t* value, const jsonl_value_t* parent, size_t value_buffer_size, jsonl_value_t* value_buffer);

static int jsonl_internal_set_values_create_path_contains_second_path(size_t path_index, const jsonl_set_value_t* set_value_table, size_t depth, size_t second_path_index);

static int jsonl_internal_set_values_create_path_sub_path_not_already_listed(size_t set_value_index, const jsonl_set_value_t* set_value_table, size_t depth);

static size_t jsonl_internal_set_values_create_path(size_t path_index, size_t set_value_count, const jsonl_set_value_t* set_value_table, size_t value_buffer_size, jsonl_value_t* value_buffer, size_t depth, const jsonl_value_t* parent);

static int jsonl_internal_set_values_input_path_compare(const jsonl_set_value_t* input_path_a, const jsonl_set_value_t* input_path_b, size_t depth);

static int jsonl_internal_set_values_is_set_value_in_path(const jsonl_internal_path_t* internal_path, const jsonl_set_value_t* input_path);

static size_t jsonl_internal_set_values(size_t set_value_count, const jsonl_set_value_t* set_value_table, jsonl_internal_path_t* path, const jsonl_value_t* parent, size_t value_buffer_size, jsonl_value_t* value_buffer);

static size_t jsonl_white_space_length(size_t json_text_size, const char* json_text)
{
	const char* end = json_text + json_text_size;
	const char* read = json_text;
	while (read != end && jsonl_is_white_space(*read))
		++read;
	return (size_t)((uintptr_t)read - (uintptr_t)json_text);
}

static size_t jsonl_printable_space_length(size_t json_text_size, const char* json_text)
{
	const char* end = json_text + json_text_size;
	const char* read = json_text;
	while (read != end && !jsonl_is_white_space(*read))
		++read;
	return (size_t)((uintptr_t)read - (uintptr_t)json_text);
}

static size_t jsonl_quoted_string_lenght(size_t json_text_size, const char* json_text)
{
	const char* end = json_text + json_text_size;
	const char* read = json_text + 1;
	if (!json_text_size || *json_text != '\"')
		return 0;
	for (int escape = 0; read != end;)
	{
		char character = *read++;
		if (!escape && character == '\"')
			return (size_t)((uintptr_t)read - (uintptr_t)json_text);
		escape = character == '\\';
	}
	return 0;
}

static size_t jsonl_decode_boolean(size_t json_text_size, const char* json_text, int* value)
{
	if ((json_text_size == 5 && json_text[0] == 'f' && json_text[1] == 'a' && json_text[2] == 'l' && json_text[3] == 's' && json_text[4] == 'e') ||
		(json_text_size > 5 && json_text[0] == 'f' && json_text[1] == 'a' && json_text[2] == 'l' && json_text[3] == 's' && json_text[4] == 'e' &&
		(jsonl_is_white_space(json_text[5]) || jsonl_is_structural_character(json_text[5]))))
	{
		if (value)
			*value = 0;
		return 5;
	}
	else if ((json_text_size == 4 && json_text[0] == 't' && json_text[1] == 'r' && json_text[2] == 'u' && json_text[3] == 'e') ||
		(json_text_size > 4 && json_text[0] == 't' && json_text[1] == 'r' && json_text[2] == 'u' && json_text[3] == 'e' &&
		(jsonl_is_white_space(json_text[4]) || jsonl_is_structural_character(json_text[4]))))
	{
		if (value)
			*value = 1;
		return 4;
	}
	else
		return 0;
}

static size_t jsonl_decode_quoted_string(size_t json_text_size, const char* json_text, size_t string_buffer_size, char* string_buffer, size_t* string_size)
{
	json_text_size = jsonl_quoted_string_lenght(json_text_size, json_text);
	const char* end = json_text + json_text_size - 1;
	const char* read = json_text + 1;
	size_t string_length = 0;
	if (!json_text_size)
		return 0;
	json_text_size -= 2;
	json_text += 1;
	while (read != end)
	{
		char character = *read;
		if (character != '\\' || read + 1 == end)
		{
			++read;
			if (string_length < string_buffer_size)
				*string_buffer++ = character;
			++string_length;
		}
		else
		{
			character = *(read + 1);
			switch (character)
			{
				case '\'' :
					read += 2;
					if (string_length < string_buffer_size)
						*string_buffer++ = '\'';
					++string_length;
					break;
				case '"' :
					read += 2;
					if (string_length < string_buffer_size)
						*string_buffer++ = '"';
					++string_length;
					break;
				case '\\' :
					read += 2;
					if (string_length < string_buffer_size)
						*string_buffer++ = '\\';
					++string_length;
					break;
				case 'n':
					read += 2;
					if (string_length < string_buffer_size)
						*string_buffer++ = '\n';
					++string_length;
					break;
				case 'r':
					read += 2;
					if (string_length < string_buffer_size)
						*string_buffer++ = '\r';
					++string_length;
					break;
				case 't':
					read += 2;
					if (string_length < string_buffer_size)
						*string_buffer++ = '\t';
					++string_length;
					break;
				case 'b':
					read += 2;
					if (string_length < string_buffer_size)
						*string_buffer++ = '\b';
					++string_length;
					break;
				case 'f':
					read += 2;
					if (string_length < string_buffer_size)
						*string_buffer++ = '\f';
					++string_length;
					break;
				case 'u':
					if (read + 5 != end && jsonl_is_hex_value(*(read + 2)) && jsonl_is_hex_value(*(read + 3)) && jsonl_is_hex_value(*(read + 4)) && jsonl_is_hex_value(*(read + 5)))
					{
						uint32_t unicode_character = ((uint32_t)jsonl_hex_value(*(read + 2)) << 12) | ((uint32_t)jsonl_hex_value(*(read + 3)) << 8) | ((uint32_t)jsonl_hex_value(*(read + 4)) << 4) | (uint32_t)jsonl_hex_value(*(read + 5));
						if (unicode_character > 0x7FF)
						{
							if (string_length < string_buffer_size)
								*(uint8_t*)string_buffer++ = (uint8_t)(unicode_character >> 12) | 0xE0;
							++string_length;
							if (string_length < string_buffer_size)
								*(uint8_t*)string_buffer++ = (uint8_t)((unicode_character >> 6) & 0x3F) | 0x80;
							++string_length;
							if (string_length < string_buffer_size)
								*(uint8_t*)string_buffer++ = (uint8_t)(unicode_character & 0x3F) | 0x80;
							++string_length;
						}
						else if (unicode_character > 0x7F)
						{
							if (string_length < string_buffer_size)
								*(uint8_t*)string_buffer++ = (uint8_t)(unicode_character >> 6) | 0xC0;
							++string_length;
							if (string_length < string_buffer_size)
								*(uint8_t*)string_buffer++ = (uint8_t)(unicode_character & 0x3F) | 0x80;
							++string_length;
						}
						else
						{
							if (string_length < string_buffer_size)
								*(uint8_t*)string_buffer++ = (uint8_t)unicode_character;
							++string_length;
						}
						read += 6;
					}
					else
					{
						++read;
						if (string_length < string_buffer_size)
							*string_buffer++ = '\\';
						++string_length;
					}
					break;
				default:
					++read;
					if (string_length < string_buffer_size)
						*string_buffer++ = '\\';
					++string_length;
					break;
			}
		}
	}
	*string_size = string_length;
	return json_text_size + 2;
}
#ifdef JSONL_FIXED_POINT_NUMBER_FORMAT
static size_t jsonl_decode_number(size_t json_text_size, const char* json_text, jsonl_number_value_t* number_value)
{
	size_t remaining_size = json_text_size;
	const char* read = json_text;
	if (!remaining_size)
		return 0;
	int is_negative = *read == '-';
	if (is_negative)
	{
		--remaining_size;
		++read;
	}
	if (!remaining_size || !jsonl_is_decimal_value(*read))
		return 0;
	size_t integer_digit_count = 1;
	const char* integer_digits = read;
	while (integer_digit_count != remaining_size && jsonl_is_decimal_value(integer_digits[integer_digit_count]))
		++integer_digit_count;
	if (integer_digit_count > 1 && *integer_digits == '0')
		return 0;
	remaining_size -= integer_digit_count;
	read += integer_digit_count;
	size_t fraction_digit_count = 0;
	const char* fraction_digits = 0;
	if (remaining_size && *read == '.')
	{
		--remaining_size;
		++read;
		if (!remaining_size || !jsonl_is_decimal_value(*read))
			return 0;
		fraction_digit_count = 1;
		fraction_digits = read;
		while (fraction_digit_count != remaining_size && jsonl_is_decimal_value(fraction_digits[fraction_digit_count]))
			++fraction_digit_count;
		remaining_size -= fraction_digit_count;
		read += fraction_digit_count;
	}
	int exponent_is_negative = 0;
	size_t exponent_digit_count = 0;
	const char* exponent_digits = 0;
	if (remaining_size && (*read == 'E' || *read == 'e'))
	{
		--remaining_size;
		++read;
		if (!remaining_size)
			return 0;
		if (*read == '+')
		{
			--remaining_size;
			++read;
		}
		else if (*read == '-')
		{
			exponent_is_negative = 1;
			--remaining_size;
			++read;
		}
		if (!remaining_size || !jsonl_is_decimal_value(*read))
			return 0;
		exponent_digit_count = 1;
		exponent_digits = read;
		while (exponent_digit_count != remaining_size && jsonl_is_decimal_value(exponent_digits[exponent_digit_count]))
			++exponent_digit_count;
		remaining_size -= exponent_digit_count;
		read += exponent_digit_count;
	}
	if (remaining_size && !jsonl_is_white_space(*read) && !jsonl_is_structural_character(*read))
		return 0;
	if (number_value)
	{
		int integer_part_overfow = 0;
		uint64_t integer_part = 0;
		for (size_t i = 0; !integer_part_overfow && i != integer_digit_count; ++i)
		{
			uint64_t new_integer_part = integer_part * (uint64_t)10;
			if (new_integer_part / (uint64_t)10 != integer_part)
				integer_part_overfow = 1;
			integer_part = new_integer_part;
			new_integer_part = integer_part + (uint64_t)(integer_digits[i] - '0');
			if (new_integer_part < integer_part)
				integer_part_overfow = 1;
			integer_part = new_integer_part;
		}
		uint64_t fraction_scale = (uint64_t)1844674407370955161;
		uint64_t fraction_sub_scale = (uint64_t)600000000000000000;
		uint64_t fraction_part = 0;
		uint64_t fraction_sub_part = 0;
		for (size_t n = fraction_digit_count < 19 ? fraction_digit_count : 19, i = 0; i != n; ++i)
		{
			uint64_t next_digit = (uint64_t)(fraction_digits[i] - '0');
			uint64_t fraction_increment = next_digit * fraction_scale;
			uint64_t sub_fraction_increment = next_digit * fraction_sub_scale;
			uint64_t new_sub_fraction = fraction_sub_part + sub_fraction_increment;
			uint64_t new_sub_fraction_over_flow = new_sub_fraction / (uint64_t)1000000000000000000;
			fraction_part += fraction_increment + new_sub_fraction_over_flow;
			fraction_sub_part = new_sub_fraction % (uint64_t)1000000000000000000;
			fraction_sub_scale = (fraction_sub_scale / 10) + ((uint64_t)100000000000000000 * (fraction_scale % 10));
			fraction_scale = fraction_scale / 10;
		}
		if (integer_part_overfow)
		{
			integer_part = (uint64_t)0xFFFFFFFFFFFFFFFF;
			fraction_part = (uint64_t)0xFFFFFFFFFFFFFFFF;
		}
		if (exponent_digit_count)
		{
			const int max_exponent_part = 39;
			int exponent_part = 0;
			for (size_t i = 0; exponent_part < max_exponent_part && i != exponent_digit_count; ++i)
			{
				int new_exponent_part = exponent_part * 10;
				if (new_exponent_part < max_exponent_part)
				{
					exponent_part = new_exponent_part;
					new_exponent_part = exponent_part + (int)(exponent_digits[i] - '0');
					if (new_exponent_part < max_exponent_part)
						exponent_part = new_exponent_part;
					else
						exponent_part = max_exponent_part;
				}
				else
					exponent_part = max_exponent_part;
			}
			if (exponent_part < max_exponent_part)
			{
				if (exponent_is_negative)
				{
					for (int i = 0; i != exponent_part; ++i)
					{
						uint64_t new_integer_part = integer_part / (uint64_t)10;
						uint64_t new_fraction_part = (fraction_part / (uint64_t)10) + ((integer_part % (uint64_t)10) * (uint64_t)1844674407370955161);
						integer_part = new_integer_part;
						fraction_part = new_fraction_part;
					}
				}
				else
				{
					int exponent_shift_overflow = 0;
					for (int i = 0; !exponent_shift_overflow && i != exponent_part; ++i)
					{
						uint64_t new_fraction_part_high = fraction_part / (uint64_t)1844674407370955161;
						uint64_t new_fraction_part_low = (fraction_part % (uint64_t)1844674407370955161) * (uint64_t)10;
						uint64_t new_integer_part_low = integer_part * (uint64_t)10;
						fraction_part = new_fraction_part_low;
						if (new_integer_part_low < integer_part)
							exponent_shift_overflow = 1;
						integer_part = new_integer_part_low;
						new_integer_part_low += new_fraction_part_high;
						if (new_integer_part_low < integer_part)
							exponent_shift_overflow = 1;
						integer_part = new_integer_part_low;
					}
					if (exponent_shift_overflow)
					{
						integer_part = (uint64_t)0xFFFFFFFFFFFFFFFF;
						fraction_part = (uint64_t)0xFFFFFFFFFFFFFFFF;
					}
				}
			}
			else if (integer_part || fraction_part)
			{
				integer_part = (uint64_t)0xFFFFFFFFFFFFFFFF;
				fraction_part = (uint64_t)0xFFFFFFFFFFFFFFFF;
			}
		}
		number_value->sign = is_negative;
		number_value->integer = integer_part;
		number_value->fraction = fraction_part;
	}
	return (size_t)((uintptr_t)read - (uintptr_t)json_text);
}
#else
static size_t jsonl_decode_number(size_t json_text_size, const char* json_text, jsonl_number_value_t* number_value)
{
	size_t remaining_size = json_text_size;
	const char* read = json_text;
	if (!remaining_size)
		return 0;
	int is_negative = *read == '-';
	if (is_negative)
	{
		--remaining_size;
		++read;
	}
	if (!remaining_size || !jsonl_is_decimal_value(*read))
		return 0;
	size_t integer_digit_count = 1;
	const char* integer_digits = read;
	while (integer_digit_count != remaining_size && jsonl_is_decimal_value(integer_digits[integer_digit_count]))
		++integer_digit_count;
	if (integer_digit_count > 1 && *integer_digits == '0')
		return 0;
	remaining_size -= integer_digit_count;
	read += integer_digit_count;
	size_t fraction_digit_count = 0;
	const char* fraction_digits = 0;
	if (remaining_size && *read == '.')
	{
		--remaining_size;
		++read;
		if (!remaining_size || !jsonl_is_decimal_value(*read))
			return 0;
		fraction_digit_count = 1;
		fraction_digits = read;
		while (fraction_digit_count != remaining_size && jsonl_is_decimal_value(fraction_digits[fraction_digit_count]))
			++fraction_digit_count;
		remaining_size -= fraction_digit_count;
		read += fraction_digit_count;
	}
	int exponent_is_negative = 0;
	size_t exponent_digit_count = 0;
	const char* exponent_digits = 0;
	if (remaining_size && (*read == 'E' || *read == 'e'))
	{
		--remaining_size;
		++read;
		if (!remaining_size)
			return 0;
		if (*read == '+')
		{
			--remaining_size;
			++read;
		}
		else if (*read == '-')
		{
			exponent_is_negative = 1;
			--remaining_size;
			++read;
		}
		if (!remaining_size || !jsonl_is_decimal_value(*read))
			return 0;
		exponent_digit_count = 1;
		exponent_digits = read;
		while (exponent_digit_count != remaining_size && jsonl_is_decimal_value(exponent_digits[exponent_digit_count]))
			++exponent_digit_count;
		remaining_size -= exponent_digit_count;
		read += exponent_digit_count;
	}
	if (remaining_size && !jsonl_is_white_space(*read) && !jsonl_is_structural_character(*read))
		return 0;
	if (number_value)
	{
		double value = 0.0;
		double decimal_shift;
		if (fraction_digit_count)
		{
			decimal_shift = 1.0;
			for (size_t i = fraction_digit_count; i--;)
				decimal_shift /= 10.0;
			for (size_t i = fraction_digit_count; i--; decimal_shift *= 10.0)
				value += decimal_shift * (double)jsonl_decimal_value(fraction_digits[i]);
		}
		decimal_shift = 1.0;
		for (size_t i = integer_digit_count; i--; decimal_shift *= 10.0)
			value += decimal_shift * (double)jsonl_decimal_value(integer_digits[i]);
		if (exponent_digit_count)
		{
			if (exponent_digit_count < 4)
			{
				int exponent_value = 0;
				for (int i = 0; i != (int)exponent_digit_count; ++i)
					exponent_value = 10 * exponent_value + jsonl_decimal_value(exponent_digits[i]);
				if (exponent_value)
				{
					decimal_shift = 1.0;
					for (size_t i = exponent_value; i--;)
						decimal_shift *= 10.0;
					if (exponent_is_negative)
						value /= decimal_shift;
					else
						value *= decimal_shift;
				}
			}
			else
			{
				if (exponent_is_negative)
					value = 0.0;
				else
				{
					const uint64_t hex_infinity = 0x7FF0000000000000;
					value = *(const double*)&hex_infinity;
				}
			}
		}
		if (value != 0.0 && is_negative)
			value = -value;
		number_value->value = value;
	}
	return (size_t)((uintptr_t)read - (uintptr_t)json_text);
}
#endif

static size_t jsonl_object_text_size(size_t json_text_size, const char* json_text)
{
	const char* end = json_text + json_text_size;
	const char* read = json_text + 1;
	int depth = 1;
	if (!json_text_size || *json_text != '{')
		return 0;
	while (read != end)
	{
		char character = *read++;
		if (character == '}')
		{
			--depth;
			if (!depth)
				return (size_t)((uintptr_t)read - (uintptr_t)json_text);
		}
		else if (character == '{')
			++depth;
		else if (character == '"')
		{
			--read;
			size_t quoted_string_lenght = jsonl_quoted_string_lenght((size_t)((uintptr_t)end - (uintptr_t)read), read);
			if (quoted_string_lenght)
				read += quoted_string_lenght;
			else
				return 0;
		}
	}
	return 0;
}

static size_t jsonl_array_text_size(size_t json_text_size, const char* json_text)
{
	const char* end = json_text + json_text_size;
	const char* read = json_text + 1;
	int depth = 1;
	if (!json_text_size || *json_text != '[')
		return 0;
	while (read != end)
	{
		char character = *read++;
		if (character == ']')
		{
			--depth;
			if (!depth)
				return (size_t)((uintptr_t)read - (uintptr_t)json_text);
		}
		else if (character == '[')
			++depth;
		else if (character == '"')
		{
			--read;
			size_t quoted_string_lenght = jsonl_quoted_string_lenght((size_t)((uintptr_t)end - (uintptr_t)read), read);
			if (quoted_string_lenght)
				read += quoted_string_lenght;
			else
				return 0;
		}
	}
	return 0;
}

static int jsonl_decide_value_type(size_t json_text_size, const char* json_text, size_t* object_text_size)
{
	if ((json_text_size == 4 && json_text[0] == 'n' && json_text[1] == 'u' && json_text[2] == 'l' && json_text[3] == 'l') ||
		(json_text_size > 4 && json_text[0] == 'n' && json_text[1] == 'u' && json_text[2] == 'l' && json_text[3] == 'l' &&
		(jsonl_is_white_space(json_text[4]) || jsonl_is_structural_character(json_text[4]))))
	{
		*object_text_size = 4;
		return JSONL_TYPE_NULL;
	}
	size_t length = jsonl_decode_boolean(json_text_size, json_text, 0);
	if (length)
	{
		*object_text_size = length;
		return JSONL_TYPE_BOOLEAN;
	}
	length = jsonl_object_text_size(json_text_size, json_text);
	if (length)
	{
		*object_text_size = length;
		return JSONL_TYPE_OBJECT;
	}
	length = jsonl_array_text_size(json_text_size, json_text);
	if (length)
	{
		*object_text_size = length;
		return JSONL_TYPE_ARRAY;
	}
	length = jsonl_decode_number(json_text_size, json_text, 0);
	if (length)
	{
		*object_text_size = length;
		return JSONL_TYPE_NUMBER;
	}
	length = jsonl_quoted_string_lenght(json_text_size, json_text);
	if (length)
	{
		*object_text_size = length;
		return JSONL_TYPE_STRING;
	}
	*object_text_size = 0;
	return JSONL_TYPE_ERROR;
}

static size_t jsonl_name_value_pair_size(size_t json_text_size, const char* json_text, size_t* name_size, int* value_type, size_t* value_offset, size_t* value_size)
{
	size_t value_lenght;
	size_t name_lenght = jsonl_quoted_string_lenght(json_text_size, json_text);
	if (name_size)
		*name_size = name_lenght;
	const char* read = json_text;
	const char* end = json_text + json_text_size;
	if (!name_lenght)
		return 0;
	read += name_lenght;
	read += jsonl_white_space_length((size_t)((uintptr_t)end - (uintptr_t)read), read);
	if (read == end || *read != ':')
		return 0;
	read += 1;
	read += jsonl_white_space_length((size_t)((uintptr_t)end - (uintptr_t)read), read);
	if (read == end)
		return 0;
	size_t object_offset = (size_t)((uintptr_t)read - (uintptr_t)json_text);
	if (value_offset)
		*value_offset = object_offset;
	int value_object_type = jsonl_decide_value_type((size_t)((uintptr_t)end - (uintptr_t)read), read, &value_lenght);
	if (value_object_type == JSONL_TYPE_ERROR)
		return 0;
	if (value_type)
		*value_type = value_object_type;
	if (value_size)
		*value_size = value_lenght;
	return object_offset + value_lenght;
}

static size_t jsonl_count_object_name_value_pairs(size_t json_text_size, const char* json_text)
{
	json_text_size = jsonl_object_text_size(json_text_size, json_text);
	if (!json_text_size)
		return (size_t)~0;
	const char* end = json_text + json_text_size - 1;
	const char* read = json_text + 1;
	size_t object_count = 0;
	int expecting_new_object = 1;
	while (read != end)
	{
		char character = *read;
		if (expecting_new_object)
		{
			if (character == '"')
			{
				size_t name_value_pair_size = jsonl_name_value_pair_size((size_t)((uintptr_t)end - (uintptr_t)read), read, 0, 0, 0, 0);
				if (!name_value_pair_size)
					return (size_t)~0;
				read += name_value_pair_size;
				++object_count;
				expecting_new_object = 0;
			}
			else if (jsonl_is_white_space(character))
				++read;
			else
				return (size_t)~0;
		}
		else
		{
			if (character == ',')
				expecting_new_object = 1;
			else if (!jsonl_is_white_space(character))
				return (size_t)~0;
			++read;
		}
	}
	return (!object_count || !expecting_new_object) ? object_count : (size_t)~0;
}

static size_t jsonl_count_array_values(size_t json_text_size, const char* json_text)
{
	json_text_size = jsonl_array_text_size(json_text_size, json_text);
	if (!json_text_size)
		return (size_t)~0;
	const char* end = json_text + json_text_size - 1;
	const char* read = json_text + 1;
	size_t object_count = 0;
	int expecting_new_object = 1;
	while (read != end)
	{
		char character = *read;
		if (expecting_new_object)
		{
			if (jsonl_is_white_space(character))
				++read;
			else
			{
				size_t value_lenght;
				int value_type = jsonl_decide_value_type((size_t)((uintptr_t)end - (uintptr_t)read), read, &value_lenght);
				if (value_type == JSONL_TYPE_ERROR)
					return (size_t)~0;
				read += value_lenght;
				++object_count;
				expecting_new_object = 0;
			}
		}
		else
		{
			if (character == ',')
				expecting_new_object = 1;
			else if (!jsonl_is_white_space(character))
				return (size_t)~0;
			++read;
		}
	}
	return (!object_count || !expecting_new_object) ? object_count : (size_t)~0;
}

static void jsonl_terminator_string(char* string_end)
{
	*string_end++ = 0;
	while ((uintptr_t)string_end & (uintptr_t)jsonl_value_alignment_minus_one)
		*string_end++ = 0;
}

static size_t jsonl_create_tree_from_text(size_t json_text_size, const char* json_text, size_t value_buffer_size, jsonl_value_t* value_buffer, jsonl_value_t* parent_value, size_t* required_value_buffer_size)
{
	size_t value_text_size;
	int value_type = jsonl_decide_value_type(json_text_size, json_text, &value_text_size);
	int expecting_sub_value;
	int boolean_value;
	jsonl_number_value_t number_value_structure;
	size_t value_size = jsonl_round_size(sizeof(jsonl_value_t));
	size_t string_length;
	size_t string_size;
	size_t sub_value_text_size;
	size_t sub_value_count;
	size_t sub_value_size;
	uintptr_t polymorphic_content = (uintptr_t)value_buffer + value_size;
	jsonl_value_t* sub_value_content;
	const char* end = json_text + value_text_size - 1;
	const char* read = json_text + 1;
	switch (value_type)
	{
		case JSONL_TYPE_OBJECT:
			sub_value_count = jsonl_count_object_name_value_pairs(value_text_size, json_text);
			if (sub_value_count == (size_t)~0)
				return 0;
			value_size += jsonl_round_size(sub_value_count * sizeof(*value_buffer->object.table));
			sub_value_content = (jsonl_value_t*)((uintptr_t)value_buffer + value_size);
			expecting_sub_value = 1;
			if (value_size <= value_buffer_size)
			{
				value_buffer->object.value_count = sub_value_count;
				*(uintptr_t*)&value_buffer->object.table = polymorphic_content;
			}
			for (size_t sub_object_index = 0; sub_object_index != sub_value_count;)
			{
				char character = *read;
				if (expecting_sub_value)
				{
					if (character == '"')
					{
						size_t name_length;
						size_t value_offset;
						size_t value_lenght;
						size_t name_value_pair_length = jsonl_name_value_pair_size((size_t)((uintptr_t)end - (uintptr_t)read), read, &name_length, 0, &value_offset, &value_lenght);
						if (!name_value_pair_length || !jsonl_decode_quoted_string(name_length, read, (value_size <= value_buffer_size) ? (value_buffer_size - value_size) : 0, (char*)sub_value_content, &string_length))
							return 0;
						string_size = jsonl_round_size(string_length + 1);
						value_size += string_size;
						if (value_size <= value_buffer_size)
						{
							jsonl_value_t* next_sub_value_content = (jsonl_value_t*)((uintptr_t)sub_value_content + string_size);
							value_buffer->object.table[sub_object_index].name_length = string_length;
							value_buffer->object.table[sub_object_index].name = (char*)sub_value_content;
							value_buffer->object.table[sub_object_index].value = next_sub_value_content;
							jsonl_terminator_string((char*)sub_value_content + string_length);
							sub_value_content = next_sub_value_content;
						}
						size_t value_text_lenght = jsonl_create_tree_from_text(value_lenght, read + value_offset, (value_size <= value_buffer_size) ? (value_buffer_size - value_size) : 0, sub_value_content, value_buffer, &sub_value_size);
						if (!value_text_lenght)
							return 0;
						value_size += sub_value_size;
						sub_value_content = (jsonl_value_t*)((uintptr_t)sub_value_content + sub_value_size);
						++sub_object_index;
						read += name_value_pair_length;
						expecting_sub_value = 0;
					}
					else if (jsonl_is_white_space(character))
						++read;
					else
						return 0;
				}
				else
				{
					if (character == ',')
						expecting_sub_value = 1;
					else if (!jsonl_is_white_space(character))
						return 0;
					++read;
				}
			}
			if (value_size <= value_buffer_size)
			{
				value_buffer->type = value_type;
				value_buffer->size = value_size;
				value_buffer->parent = parent_value;
			}
			*required_value_buffer_size = value_size;
			return value_text_size;
		case JSONL_TYPE_ARRAY:
			sub_value_count = jsonl_count_array_values(value_text_size, json_text);
			if (sub_value_count == (size_t)~0)
				return 0;
			value_size += jsonl_round_size(sub_value_count * sizeof(jsonl_value_t*));
			sub_value_content = (jsonl_value_t*)((uintptr_t)value_buffer + value_size);
			expecting_sub_value = 1;
			for (size_t sub_object_index = 0; sub_object_index != sub_value_count;)
			{
				char character = *read;
				if (expecting_sub_value)
				{
					if (jsonl_is_white_space(character))
						++read;
					else
					{
						sub_value_text_size = jsonl_create_tree_from_text((size_t)((uintptr_t)end - (uintptr_t)read), read, (value_size <= value_buffer_size) ? (value_buffer_size - value_size) : 0, sub_value_content, value_buffer, &sub_value_size);
						if (!sub_value_text_size)
							return 0;
						if (value_size <= value_buffer_size)
						{
							*((jsonl_value_t**)polymorphic_content + sub_object_index) = sub_value_content;
							sub_value_content = (jsonl_value_t*)((uintptr_t)sub_value_content + sub_value_size);
						}
						value_size += sub_value_size;
						read += sub_value_text_size;
						++sub_object_index;
						expecting_sub_value = 0;
					}
				}
				else
				{
					if (character == ',')
						expecting_sub_value = 1;
					else if (!jsonl_is_white_space(character))
						return 0;
					++read;
				}
			}
			if (value_size <= value_buffer_size)
			{
				value_buffer->array.value_count = sub_value_count;
				value_buffer->array.table = (jsonl_value_t**)polymorphic_content;
				value_buffer->type = value_type;
				value_buffer->size = value_size;
				value_buffer->parent = parent_value;
			}
			*required_value_buffer_size = value_size;
			return value_text_size;
		case JSONL_TYPE_STRING:
			if (!jsonl_decode_quoted_string(value_text_size, json_text, (value_size <= value_buffer_size) ? (value_buffer_size - value_size) : 0, (char*)polymorphic_content, &string_length))
				return 0;
			value_size += jsonl_round_size(string_length + 1);
			if (value_size <= value_buffer_size)
			{
				jsonl_terminator_string((char*)((uintptr_t)polymorphic_content + string_length));
				value_buffer->string.length = string_length;
				value_buffer->string.value = (char*)polymorphic_content;
				value_buffer->type = value_type;
				value_buffer->size = value_size;
				value_buffer->parent = parent_value;
			}
			*required_value_buffer_size = value_size;
			return value_text_size;
		case JSONL_TYPE_NUMBER:
			if (!jsonl_decode_number(value_text_size, json_text, &number_value_structure))
				return 0;
			if (value_size <= value_buffer_size)
			{
				value_buffer->number = number_value_structure;
				value_buffer->type = value_type;
				value_buffer->size = value_size;
				value_buffer->parent = parent_value;
			}
			*required_value_buffer_size = value_size;
			return value_text_size;
		case JSONL_TYPE_BOOLEAN:
			if (!jsonl_decode_boolean(value_text_size, json_text, &boolean_value))
				return 0;
			if (value_size <= value_buffer_size)
			{
				value_buffer->boolean.value = boolean_value;
				value_buffer->type = value_type;
				value_buffer->size = value_size;
				value_buffer->parent = parent_value;
			}
			*required_value_buffer_size = value_size;
			return value_text_size;
		case JSONL_TYPE_NULL:
			if (value_size <= value_buffer_size)
			{
				value_buffer->type = value_type;
				value_buffer->size = value_size;
				value_buffer->parent = parent_value;
			}
			*required_value_buffer_size = value_size;
			return value_text_size;
		default:
			return 0;
	}
}

size_t jsonl_parse_text(size_t json_text_size, const char* json_text, size_t value_buffer_size, jsonl_value_t* value_buffer)
{
	size_t beginning_white_space = jsonl_white_space_length(json_text_size, json_text);
	size_t tree_size;
	if (jsonl_create_tree_from_text(json_text_size - beginning_white_space, json_text + beginning_white_space, value_buffer_size, value_buffer, 0, &tree_size))
		return tree_size;
	else
		return 0;
}

const jsonl_value_t* jsonl_get_value(const jsonl_value_t* parent_value, size_t path_length, const jsonl_path_component_t* path_table, int required_value_type)
{
	const jsonl_value_t* iterator = parent_value;
	for (size_t i = 0; i != path_length; ++i)
		if (path_table[i].container_type == iterator->type)
		{
			if (iterator->type == JSONL_TYPE_OBJECT)
			{
				size_t child_index = (size_t)~0;
				for (size_t n = iterator->object.value_count, j = 0; child_index == (size_t)~0 && j != n; ++j)
					if (path_table[i].name.length == iterator->object.table[j].name_length && jsonl_memory_compare(path_table[i].name.value, iterator->object.table[j].name, path_table[i].name.length))
						child_index = j;
				if (child_index == (size_t)~0)
					return 0;
				iterator = iterator->object.table[child_index].value;
			}
			else if (iterator->type == JSONL_TYPE_ARRAY)
			{
				if (path_table[i].index >= iterator->array.value_count)
					return 0;
				iterator = iterator->array.table[path_table[i].index];
			}
			else
				return 0;
		}
		else
			return 0;
	if (required_value_type && iterator->type != required_value_type)
		return 0;
	return iterator;
}

#ifdef JSONL_FIXED_POINT_NUMBER_FORMAT
static size_t jsonl_print_number_value(const jsonl_value_t* number_value, size_t text_buffer_size, char* text_buffer)
{
	int sign_bit = number_value->number.sign ? 1 : 0;
	uint64_t decimal_part = number_value->number.integer;
	uint64_t fraction_part = number_value->number.fraction;

	uint64_t decimal_shift = 1;
	int decimal_count = 1;
	while (decimal_count < 19 && decimal_part / (decimal_shift * 10))
	{
		++decimal_count;
		decimal_shift *= 10;
	}
	
	uint64_t fraction_div = 1844674407370955161;
	int fraction_count = 0;
	for (int i = 0; i != 16; ++i)
	{
		uint64_t fraction_digit = fraction_part / fraction_div;
		if (fraction_digit % 10)
			fraction_count = i + 1;
		fraction_div /= 10;
	}

	if (text_buffer_size >= (size_t)sign_bit + (size_t)decimal_count + (fraction_count ? (1 + (size_t)fraction_count) : 0))
	{
		if (sign_bit)
			*text_buffer++ = '-';

		for (int digit = 0; digit != decimal_count; ++digit)
		{
			*text_buffer++ = '0' + (char)((decimal_part / decimal_shift) % 10);
			decimal_shift /= 10;
		}

		if (fraction_count)
		{
			*text_buffer++ = '.';
			fraction_div = 1844674407370955161;
			for (int i = 0; i != fraction_count; ++i)
			{
				uint64_t fraction_digit = fraction_part / fraction_div;
				*text_buffer++ = '0' + (fraction_digit % 10);
				fraction_div /= 10;
			}
		}
	}

	return (size_t)sign_bit + (size_t)decimal_count + (fraction_count ? (1 + (size_t)fraction_count) : 0);
}
#else
static size_t jsonl_print_number_value(const jsonl_value_t* number_value, size_t text_buffer_size, char* text_buffer)
{
	double value = number_value->number.value;
	int sign_bit = (int)(value < 0.0);
	if (sign_bit)
		value = -value;

	uint64_t decimal_part = (uint64_t)value;
	uint64_t decimal_shift = 1;
	int decimal_count = 1;
	while (decimal_count < 19 && decimal_part / (decimal_shift * 10))
	{
		++decimal_count;
		decimal_shift *= 10;
	}

	uint64_t fraction_part = (uint64_t)((value - (double)decimal_part) * 10000000000000000000.0);
	int fraction_count;
	if (fraction_part)
	{
		fraction_count = 1;
		uint64_t fraction_shift = (uint64_t)100000000000000000;
		for (int digit = 1; digit != 19; ++digit)
		{
			if ((fraction_part / fraction_shift) % 10)
				fraction_count = digit + 1;
			fraction_shift /= 10;
		}
	}
	else
		fraction_count = 0;

	if (text_buffer_size >= (size_t)sign_bit + (size_t)decimal_count + (fraction_count ? (1 + (size_t)fraction_count) : 0))
	{
		if (sign_bit)
			*text_buffer++ = '-';

		for (int digit = 0; digit != decimal_count; ++digit)
		{
			*text_buffer++ = '0' + (char)((decimal_part / decimal_shift) % 10);
			decimal_shift /= 10;
		}

		if (fraction_count)
		{
			*text_buffer++ = '.';

			uint64_t fraction_shift = (uint64_t)1000000000000000000;
			for (int digit = 0; digit != fraction_count; ++digit)
			{
				*text_buffer++ = '0' + (char)((fraction_part / fraction_shift) % 10);
				fraction_shift /= 10;
			}
		}
	}

	return (size_t)sign_bit + (size_t)decimal_count + (fraction_count ? (1 + (size_t)fraction_count) : 0);
}
#endif

static size_t jsonl_print_string_value(size_t string_size, const char* string, size_t text_buffer_size, char* text_buffer)
{
	static const struct { char character; char escaped; } escaped_character_table[] = {
				{ '\"', '\"' },
				{ '\\', '\\' },
				{ '\b', 'b' },
				{ '\f', 'f' },
				{ '\n', 'n' },
				{ '\r', 'r' },
				{ '\t', 't' } };
	if (text_buffer_size)
		text_buffer[0] = '\"';
	size_t printed_string_length = 0;
	for (size_t i = 0; i != string_size; ++i)
	{
		char character = string[i];
		int escaped_character_index = -1;
		for (int j = 0; escaped_character_index == -1 && j != (int)(sizeof(escaped_character_table) / sizeof(*escaped_character_table)); ++j)
			if (character == escaped_character_table[j].character)
				escaped_character_index = j;
		if (escaped_character_index == -1)
		{
			if (character < 0x20)
			{
				if (printed_string_length + 5 < text_buffer_size)
				{
					text_buffer[1 + printed_string_length] = '\\';
					text_buffer[1 + printed_string_length + 1] = 'u';
					text_buffer[1 + printed_string_length + 2] = '0';
					text_buffer[1 + printed_string_length + 3] = '0';
					text_buffer[1 + printed_string_length + 4] = (char)((int)'0' + ((int)character >> 4));
					text_buffer[1 + printed_string_length + 5] = (((int)character & 0xF) < 0xA) ? (char)((int)'0' + ((int)character & 0xF)) : (char)(((int)'A' - 10) + ((int)character & 0xF));
				}
				printed_string_length += 6;
			}
			else
			{
				if (printed_string_length < text_buffer_size)
					text_buffer[1 + printed_string_length] = character;
				++printed_string_length;
			}
		}
		else
		{
			if (printed_string_length + 1 < text_buffer_size)
			{
				text_buffer[1 + printed_string_length] = '\\';
				text_buffer[1 + printed_string_length + 1] = escaped_character_table[escaped_character_index].character;
			}
			printed_string_length += 2;
		}
	}
	if (1 + printed_string_length < text_buffer_size)
		text_buffer[1 + printed_string_length] = '\"';
	return 2 + printed_string_length;
}

static void jsonl_print_indent(char* text_buffer, size_t depth)
{
	for (char* end = text_buffer + depth; text_buffer != end; ++text_buffer)
		*text_buffer = '\t';
}

static size_t jsonl_internal_print(const jsonl_value_t* value_tree, size_t json_text_buffer_size, char* json_text_buffer, size_t depth)
{
	switch (value_tree->type)
	{
		case JSONL_TYPE_OBJECT:
		{
			if (1 < json_text_buffer_size)
			{
				json_text_buffer[0] = '{';
				json_text_buffer[1] = '\n';
			}
			size_t object_size = 2;
			for (size_t i = 0; i != value_tree->object.value_count; ++i)
			{
				if (object_size + 1 < json_text_buffer_size)
					jsonl_print_indent(json_text_buffer + object_size, depth + 1);
				object_size += depth + 1;
				object_size += jsonl_print_string_value(value_tree->object.table[i].name_length, value_tree->object.table[i].name, (object_size < json_text_buffer_size) ? (json_text_buffer_size - object_size) : 0, json_text_buffer + object_size);
				if (object_size + 2 < json_text_buffer_size)
				{
					json_text_buffer[object_size] = ' ';
					json_text_buffer[object_size + 1] = ':';
					json_text_buffer[object_size + 2] = ' ';
				}
				object_size += 3;
				size_t child_value_size = jsonl_internal_print(value_tree->object.table[i].value, (object_size < json_text_buffer_size) ? (json_text_buffer_size - object_size) : 0, json_text_buffer + object_size, depth + 1);
				if (!child_value_size)
					return 0;
				object_size += child_value_size;
				if (i + 1 != value_tree->object.value_count)
				{
					if (object_size < json_text_buffer_size)
						json_text_buffer[object_size] = ',';
					++object_size;
				}
				if (object_size < json_text_buffer_size)
					json_text_buffer[object_size] = '\n';
				++object_size;
			}
			if (object_size + depth < json_text_buffer_size)
			{
				jsonl_print_indent(json_text_buffer + object_size, depth);
				json_text_buffer[object_size + depth] = '}';
			}
			object_size += depth + 1;
			return object_size;
		}
		case JSONL_TYPE_ARRAY:
		{
			if (1 < json_text_buffer_size)
			{
				json_text_buffer[0] = '[';
				json_text_buffer[1] = '\n';
			}
			size_t array_size = 2;
			for (size_t i = 0; i != value_tree->array.value_count; ++i)
			{
				if (array_size + 1 < json_text_buffer_size)
					jsonl_print_indent(json_text_buffer + array_size, depth + 1);
				array_size += depth + 1;
				size_t child_value_size = jsonl_internal_print(value_tree->array.table[i], (array_size < json_text_buffer_size) ? (json_text_buffer_size - array_size) : 0, json_text_buffer + array_size, depth + 1);
				if (!child_value_size)
					return 0;
				array_size += child_value_size;
				if (i + 1 != value_tree->array.value_count)
				{
					if (array_size < json_text_buffer_size)
						json_text_buffer[array_size] = ',';
					++array_size;
				}
				if (array_size < json_text_buffer_size)
					json_text_buffer[array_size] = '\n';
				++array_size;
			}
			if (array_size + depth < json_text_buffer_size)
			{
				jsonl_print_indent(json_text_buffer + array_size, depth);
				json_text_buffer[array_size + depth] = ']';
			}
			array_size += depth + 1;
			return array_size;
		}
		case JSONL_TYPE_STRING:
			return jsonl_print_string_value(value_tree->string.length, value_tree->string.value, json_text_buffer_size, json_text_buffer);
		case JSONL_TYPE_NUMBER:
			return jsonl_print_number_value(value_tree, json_text_buffer_size, json_text_buffer);
		case JSONL_TYPE_BOOLEAN:
		{
			if (value_tree->boolean.value)
			{
				if (json_text_buffer_size > 3)
				{
					json_text_buffer[0] = 't';
					json_text_buffer[1] = 'r';
					json_text_buffer[2] = 'u';
					json_text_buffer[3] = 'e';
				}
				return 4;
			}
			else
			{
				if (json_text_buffer_size > 4)
				{
					json_text_buffer[0] = 'f';
					json_text_buffer[1] = 'a';
					json_text_buffer[2] = 'l';
					json_text_buffer[3] = 's';
					json_text_buffer[4] = 'e';
				}
				return 5;
			}
		}
		case JSONL_TYPE_NULL:
		{
			if (json_text_buffer_size > 3)
			{
				json_text_buffer[0] = 'n';
				json_text_buffer[1] = 'u';
				json_text_buffer[2] = 'l';
				json_text_buffer[3] = 'l';
			}
			return 4;
		}
		default:
			return 0;
	}
}

size_t jsonl_print(const jsonl_value_t* value_tree, size_t json_text_buffer_size, char* json_text_buffer)
{
	return jsonl_internal_print(value_tree, json_text_buffer_size, json_text_buffer, 0);
}

static size_t jsonl_internal_create_null_value(const jsonl_value_t* parent, size_t value_buffer_size, jsonl_value_t* value_buffer)
{
	size_t null_object_size = jsonl_round_size(sizeof(jsonl_value_t));
	if (null_object_size <= value_buffer_size)
	{
		value_buffer->size = null_object_size;
		value_buffer->parent = (jsonl_value_t*)parent;
		value_buffer->type = JSONL_TYPE_NULL;
	}
	return null_object_size;
}

static size_t jsonl_internal_copy_value(const jsonl_value_t* value, const jsonl_value_t* parent, size_t value_buffer_size, jsonl_value_t* value_buffer)
{
	size_t object_size = jsonl_round_size(sizeof(jsonl_value_t));
	switch (value->type)
	{
		case JSONL_TYPE_OBJECT:
		{
			size_t object_table_size = jsonl_round_size(value->object.value_count * sizeof(*value->object.table));
			if (object_size <= value_buffer_size)
			{
				value_buffer->object.value_count = value->object.value_count;
				*(void**)&value_buffer->object.table = (void*)((uintptr_t)value_buffer + object_size);
			}
			for (size_t i = 0; i != value->object.value_count; ++i)
			{
				size_t name_size = jsonl_round_size(value->object.table[i].name_length + 1);
				if (object_size + object_table_size + name_size <= value_buffer_size)
				{
					value_buffer->object.table[i].name_length = value->object.table[i].name_length;
					value_buffer->object.table[i].name = (char*)((uintptr_t)value_buffer + object_size + object_table_size);
					jsonl_copy_memory((void*)((uintptr_t)value_buffer + object_size + object_table_size), value->object.table[i].name, value->object.table[i].name_length + 1);
				}
				object_table_size += name_size;
			}
			object_size += object_table_size;
			for (size_t i = 0; i != value->object.value_count; ++i)
			{
				if (object_size <= value_buffer_size)
					value_buffer->object.table[i].value = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
				size_t remaining_buffer_size = object_size < value_buffer_size ? value_buffer_size - object_size : 0;
				size_t sub_value_size = jsonl_internal_copy_value(value->object.table[i].value, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
				if (!sub_value_size)
					return 0;
				object_size += sub_value_size;
			}
			break;
		}
		case JSONL_TYPE_ARRAY:
		{
			size_t array_table_size = jsonl_round_size(value->array.value_count * sizeof(jsonl_value_t*));
			if (object_size <= value_buffer_size)
			{
				value_buffer->array.value_count = value->array.value_count;
				*(void**)&value_buffer->array.table = (void*)((uintptr_t)value_buffer + object_size);
			}
			object_size += array_table_size;
			for (size_t i = 0; i != value->array.value_count; ++i)
			{
				if (object_size <= value_buffer_size)
					value_buffer->array.table[i] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
				size_t remaining_buffer_size = object_size < value_buffer_size ? value_buffer_size - object_size : 0;
				size_t sub_value_size = jsonl_internal_copy_value(value->array.table[i], value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
				if (!sub_value_size)
					return 0;
				object_size += sub_value_size;
			}
			break;
		}
		case JSONL_TYPE_STRING:
		{
			size_t data_size = jsonl_round_size(value->string.length + 1);
			if (object_size + data_size <= value_buffer_size)
			{
				value_buffer->string.length = value->string.length;
				value_buffer->string.value = (char*)((uintptr_t)value_buffer + object_size);
				jsonl_copy_memory((char*)((uintptr_t)value_buffer + object_size), value->string.value, value->string.length + 1);
			}
			object_size += data_size;
			break;
		}
		case JSONL_TYPE_NUMBER:
		{
			if (object_size <= value_buffer_size)
			{
#ifdef JSONL_FIXED_POINT_NUMBER_FORMAT
				value_buffer->number.sign = value->number.sign;
				value_buffer->number.integer = value->number.integer;
				value_buffer->number.fraction = value->number.fraction;
#else
				value_buffer->number.value = value->number.value;
#endif
			}
			break;
		}
		case JSONL_TYPE_BOOLEAN:
		{
			if (object_size <= value_buffer_size)
				value_buffer->boolean.value = value->boolean.value;
			break;
		}
		case JSONL_TYPE_NULL:
		{
			break;
		}
		default:
			return 0;
	}

	if (object_size <= value_buffer_size)
	{
		value_buffer->size = object_size;
		value_buffer->parent = (jsonl_value_t*)parent;
		value_buffer->type = value->type;
	}
	return object_size;
}

static int jsonl_internal_set_values_create_path_contains_second_path(size_t path_index, const jsonl_set_value_t* set_value_table, size_t depth, size_t second_path_index)
{
	if (path_index == second_path_index)
		return 1;
	int path_contains_this = set_value_table[path_index].path_length >= depth + 1 && set_value_table[second_path_index].path_length >= depth + 1;
	for (size_t i = 0; path_contains_this && i != depth; ++i)
	{
		if (set_value_table[path_index].path[i].container_type == set_value_table[second_path_index].path[i].container_type)
		{
			if (set_value_table[path_index].path[i].container_type == JSONL_TYPE_OBJECT)
			{
				if (set_value_table[path_index].path[i].name.length != set_value_table[second_path_index].path[i].name.length || !jsonl_memory_compare(set_value_table[path_index].path[i].name.value, set_value_table[second_path_index].path[i].name.value, set_value_table[path_index].path[i].name.length))
					path_contains_this = 0;
			}
			else if (set_value_table[path_index].path[i].container_type == JSONL_TYPE_ARRAY)
			{
				if (set_value_table[path_index].path[i].index == (size_t)~0)
					return 0;
				if (set_value_table[path_index].path[i].index != set_value_table[second_path_index].path[i].index)
					path_contains_this = 0;
			}
			else
				JSONL_ASSERT(0);
		}
		else
			path_contains_this = 0;
	}
	return path_contains_this;
}

static int jsonl_internal_set_values_create_path_sub_path_not_already_listed(size_t set_value_index, const jsonl_set_value_t* set_value_table, size_t depth)
{
	int not_already_listed = 1;
	for (size_t i = 0; not_already_listed && i != set_value_index; ++i)
	{
		if (set_value_table[i].value && set_value_table[i].path_length > depth && set_value_table[set_value_index].path[depth].container_type == set_value_table[i].path[depth].container_type)
		{
			JSONL_ASSERT(set_value_table[set_value_index].value && set_value_table[i].value);
			JSONL_ASSERT(set_value_table[set_value_index].path_length > depth && set_value_table[i].path_length > depth);
			if (set_value_table[set_value_index].path[depth].container_type == JSONL_TYPE_OBJECT)
			{
				if (set_value_table[set_value_index].path[depth].name.length == set_value_table[i].path[depth].name.length && jsonl_memory_compare(set_value_table[set_value_index].path[depth].name.value, set_value_table[i].path[depth].name.value, set_value_table[set_value_index].path[depth].name.length))
					not_already_listed = 0;
			}
			else if (set_value_table[set_value_index].path[depth].container_type == JSONL_TYPE_ARRAY)
			{
				if (set_value_table[set_value_index].path[depth].index == (size_t)~0)
					return 1;
				if (set_value_table[set_value_index].path[depth].index == set_value_table[i].path[depth].index)
					not_already_listed = 0;
			}
			else
				JSONL_ASSERT(0);
		}
	}
	return not_already_listed;
}

static size_t jsonl_internal_set_values_create_path(size_t path_index, size_t set_value_count, const jsonl_set_value_t* set_value_table, size_t value_buffer_size, jsonl_value_t* value_buffer, size_t depth, const jsonl_value_t* parent)
{
	JSONL_ASSERT(set_value_table[path_index].value && set_value_table[path_index].path_length > depth && set_value_table[path_index].path[depth].container_type == JSONL_TYPE_OBJECT || set_value_table[path_index].path[depth].container_type == JSONL_TYPE_ARRAY);

	size_t value_count = 0;
	size_t path_count = 0;
	size_t sub_value_count = 0;
	size_t append_offset = 0;
	size_t append_count = 0;
	for (size_t i = 0; i != set_value_count; ++i)
		if (set_value_table[i].value && jsonl_internal_set_values_create_path_contains_second_path(path_index, set_value_table, depth, i))
		{
			JSONL_ASSERT(set_value_table[i].path_length > depth);
			if (set_value_table[i].path_length > depth + 1)
			{
				if (jsonl_internal_set_values_create_path_sub_path_not_already_listed(i, set_value_table, depth))
				{
					if (set_value_table[path_index].path[depth].container_type == JSONL_TYPE_ARRAY)
					{
						if (set_value_table[i].path[depth].index == (size_t)~0)
							++append_count;
						else if (set_value_table[i].path[depth].index + 1 > sub_value_count)
							append_offset = set_value_table[i].path[depth].index + 1;
					}
					++path_count;
				}
			}
			else
			{
				if (set_value_table[path_index].path[depth].container_type == JSONL_TYPE_ARRAY)
				{
					if (set_value_table[i].path[depth].index == (size_t)~0)
						++append_count;
					else if (set_value_table[i].path[depth].index + 1 > sub_value_count)
						append_offset = set_value_table[i].path[depth].index + 1;
				}
				++value_count;
			}
		}
	if (set_value_table[path_index].path[depth].container_type == JSONL_TYPE_OBJECT)
		sub_value_count = value_count + path_count;
	else if (set_value_table[path_index].path[depth].container_type == JSONL_TYPE_ARRAY)
		sub_value_count = append_offset + append_count;
	else
		JSONL_ASSERT(0);

	JSONL_ASSERT(sub_value_count > 0);
	JSONL_ASSERT(value_count + path_count > 0);

	size_t object_size = jsonl_round_size(sizeof(jsonl_value_t));
	if (set_value_table[path_index].path[depth].container_type == JSONL_TYPE_OBJECT)
	{
		size_t object_table_size = jsonl_round_size(sub_value_count * sizeof(*value_buffer->object.table));
		if (object_size <= value_buffer_size)
		{
			value_buffer->object.value_count = sub_value_count;
			*(void**)&value_buffer->object.table = (void*)((uintptr_t)value_buffer + object_size);
		}
		for (size_t c = 0, i = 0; c != sub_value_count; ++i)
		{
			JSONL_ASSERT(i < set_value_count);
			if (set_value_table[i].value && jsonl_internal_set_values_create_path_contains_second_path(path_index, set_value_table, depth, i) && ((set_value_table[i].path_length + 1) == depth || jsonl_internal_set_values_create_path_sub_path_not_already_listed(i, set_value_table, depth)))
			{
				JSONL_ASSERT(set_value_table[i].path_length > depth);
				JSONL_ASSERT(set_value_table[i].path[depth].container_type == JSONL_TYPE_OBJECT);
				size_t name_length = set_value_table[i].path[depth].name.length;
				size_t name_size = jsonl_round_size(name_length + 1);
				if (object_size + object_table_size + name_size <= value_buffer_size)
				{
					value_buffer->object.table[c].name_length = name_length;
					value_buffer->object.table[c].name = (char*)((uintptr_t)value_buffer + object_size + object_table_size);
					jsonl_copy_memory((void*)((uintptr_t)value_buffer + object_size + object_table_size), set_value_table[i].path[depth].name.value, name_length + 1);
				}
				object_table_size += name_size;
				++c;
			}
		}
		object_size += object_table_size;
		for (size_t c = 0, i = 0; c != sub_value_count; ++i)
		{
			JSONL_ASSERT(i < set_value_count);
			if (set_value_table[i].value && jsonl_internal_set_values_create_path_contains_second_path(path_index, set_value_table, depth, i))
				if (set_value_table[i].path_length > depth + 1)
				{
					if (jsonl_internal_set_values_create_path_sub_path_not_already_listed(i, set_value_table, depth))
					{
						if (object_size <= value_buffer_size)
							value_buffer->object.table[c].value = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
						size_t remaining_buffer_size = object_size < value_buffer_size ? value_buffer_size - object_size : 0;
						size_t sub_value_size = jsonl_internal_set_values_create_path(i, set_value_count, set_value_table, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size), depth + 1, value_buffer);
						if (!sub_value_size)
							return 0;
						object_size += sub_value_size;
						++c;
					}
				}
				else
				{
					if (object_size <= value_buffer_size)
						value_buffer->object.table[c].value = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
					size_t remaining_buffer_size = object_size < value_buffer_size ? value_buffer_size - object_size : 0;
					size_t sub_value_size = jsonl_internal_copy_value(set_value_table[i].value, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
					if (!sub_value_size)
						return 0;
					object_size += sub_value_size;
					++c;
				}
		}
	}
	else if (set_value_table[path_index].path[depth].container_type == JSONL_TYPE_ARRAY)
	{
		size_t array_table_size = jsonl_round_size(sub_value_count * sizeof(jsonl_value_t*));
		if (object_size <= value_buffer_size)
		{
			value_buffer->array.value_count = sub_value_count;
			*(void**)&value_buffer->array.table = (void*)((uintptr_t)value_buffer + object_size);
			if ((object_size + array_table_size) <= value_buffer_size)
				for (size_t i = 0; i != sub_value_count; ++i)
					value_buffer->array.table[i] = 0;
		}
		object_size += array_table_size;
		for (size_t n = value_count + path_count, c = 0, i = 0; c != n; ++i)
		{
			JSONL_ASSERT(i < set_value_count);
			if (set_value_table[i].value && jsonl_internal_set_values_create_path_contains_second_path(path_index, set_value_table, depth, i))
				if (set_value_table[i].path_length > depth + 1)
				{
					if (jsonl_internal_set_values_create_path_sub_path_not_already_listed(i, set_value_table, depth))
					{
						JSONL_ASSERT(set_value_table[i].path[depth].container_type == JSONL_TYPE_ARRAY && (set_value_table[i].path[depth].index == (size_t)~0 || set_value_table[i].path[depth].index < sub_value_count));
						if (object_size <= value_buffer_size)
							value_buffer->array.table[(set_value_table[i].path[depth].index != (size_t)~0) ? set_value_table[i].path[depth].index : append_offset++] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
						size_t remaining_buffer_size = object_size < value_buffer_size ? value_buffer_size - object_size : 0;
						size_t sub_value_size = jsonl_internal_set_values_create_path(i, set_value_count, set_value_table, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size), depth + 1, value_buffer);
						if (!sub_value_size)
							return 0;
						object_size += sub_value_size;
						++c;
					}
				}
				else
				{
					JSONL_ASSERT(set_value_table[i].path[depth].container_type == JSONL_TYPE_ARRAY && (set_value_table[i].path[depth].index == (size_t)~0 || set_value_table[i].path[depth].index < sub_value_count));
					if (object_size <= value_buffer_size)
						value_buffer->array.table[(set_value_table[i].path[depth].index != (size_t)~0) ? set_value_table[i].path[depth].index : append_offset++] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
					size_t remaining_buffer_size = object_size < value_buffer_size ? value_buffer_size - object_size : 0;
					size_t sub_value_size = jsonl_internal_copy_value(set_value_table[i].value, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
					if (!sub_value_size)
						return 0;
					object_size += sub_value_size;
					++c;
				}
		}
		if (object_size <= value_buffer_size)
		{
			for (size_t i = 0; i != sub_value_count; ++i)
				if (!value_buffer->array.table[i])
				{
					value_buffer->array.table[i] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
					size_t remaining_buffer_size = object_size < value_buffer_size ? value_buffer_size - object_size : 0;
					size_t sub_value_size = jsonl_internal_create_null_value(value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
					object_size += sub_value_size;
				}
		}
		else
		{
			size_t sub_null_value_size = jsonl_internal_create_null_value(value_buffer, 0, 0);
			object_size += (sub_value_count - (value_count + path_count)) * sub_null_value_size;
		}
		object_size += array_table_size;
	}
	else
		JSONL_ASSERT(0);

	if (object_size <= value_buffer_size)
	{
		value_buffer->size = object_size;
		value_buffer->parent = (jsonl_value_t*)parent;
		value_buffer->type = set_value_table[path_index].path[depth].container_type;
	}
	return object_size;
}

static int jsonl_internal_set_values_input_path_compare(const jsonl_set_value_t* input_path_a, const jsonl_set_value_t* input_path_b, size_t depth)
{
	JSONL_ASSERT(input_path_a != input_path_b);
	int match = input_path_a->path_length >= depth && input_path_b->path_length >= depth;
	for (size_t i = 0; match && i != depth; ++i)
		if (input_path_a->path[i].container_type == input_path_b->path[i].container_type)
		{
			if (input_path_a->path[i].container_type == JSONL_TYPE_OBJECT)
			{
				if (input_path_a->path[i].name.length != input_path_b->path[i].name.length || !jsonl_memory_compare(input_path_a->path[i].name.value, input_path_b->path[i].name.value, input_path_a->path[i].name.length))
					match = 0;
			}
			else if (input_path_a->path[i].container_type == JSONL_TYPE_ARRAY)
			{
				if (input_path_a->path[i].index == (size_t)~0 || input_path_b->path[i].index == (size_t)~0 || input_path_a->path[i].index != input_path_b->path[i].index)
					match = 0;
			}
			else
				JSONL_ASSERT(0);
		}
		else
			match = 0;
	return match;
}

static int jsonl_internal_set_values_is_set_value_in_path(const jsonl_internal_path_t* internal_path, const jsonl_set_value_t* input_path)
{
	int value_path_match = input_path->path_length >= internal_path->depth;
	for (size_t i = internal_path->depth; value_path_match && i--; internal_path = internal_path->parent)
		if (internal_path->path_component.container_type == input_path->path[i].container_type)
		{
			if (internal_path->path_component.container_type == JSONL_TYPE_OBJECT)
			{
				if (internal_path->path_component.name.length != input_path->path[i].name.length || !jsonl_memory_compare(internal_path->path_component.name.value, input_path->path[i].name.value, internal_path->path_component.name.length))
					value_path_match = 0;
			}
			else if (internal_path->path_component.container_type == JSONL_TYPE_ARRAY)
			{
				JSONL_ASSERT(internal_path->path_component.index != (size_t)~0);
				if (internal_path->path_component.index != input_path->path[i].index)
					value_path_match = 0;
			}
			else
				JSONL_ASSERT(0);
		}
		else
			value_path_match = 0;
	return value_path_match;
}

static size_t jsonl_internal_set_values(size_t set_value_count, const jsonl_set_value_t* set_value_table, jsonl_internal_path_t* path, const jsonl_value_t* parent, size_t value_buffer_size, jsonl_value_t* value_buffer)
{
	const jsonl_value_t* source_value = path->source_value;
	JSONL_ASSERT(source_value && source_value->type == path->value_type);
	size_t indirect_overwrite_set_value_index = (size_t)~0;
	for (size_t i = 0; i != set_value_count; ++i)
		if (set_value_table[i].value)
		{
			int value_path_match = jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + i);
			if (value_path_match)
			{
				if (set_value_table[i].path_length == path->depth)
				{
					return jsonl_internal_copy_value(set_value_table[i].value, parent, value_buffer_size, value_buffer);
				}
				else if (indirect_overwrite_set_value_index == (size_t)~0)
				{
					int set_value_in_child_value = 0;
					if (set_value_table[i].path_length > path->depth && source_value->type == set_value_table[i].path[path->depth].container_type)
					{
						if (source_value->type == JSONL_TYPE_OBJECT)
						{
							for (size_t j = 0; !set_value_in_child_value && j != source_value->object.value_count; ++j)
								if (source_value->object.table[j].name_length == set_value_table[i].path[path->depth].name.length && jsonl_memory_compare(source_value->object.table[j].name, set_value_table[i].path[path->depth].name.value, source_value->object.table[j].name_length))
									set_value_in_child_value = 1;
						}
						else if (source_value->type == JSONL_TYPE_ARRAY)
						{
							if (source_value->array.value_count > set_value_table[i].path[path->depth].index)
								set_value_in_child_value = 1;
						}
						else
							JSONL_ASSERT(0);
					}
					if (!set_value_in_child_value && path->value_type != set_value_table[i].path[path->depth].container_type)
					{

						indirect_overwrite_set_value_index = i;
					}
				}
			}
		}
	if (indirect_overwrite_set_value_index != (size_t)~0)
		return jsonl_internal_set_values_create_path(indirect_overwrite_set_value_index, set_value_count, set_value_table, value_buffer_size, value_buffer, path->depth, parent);

	jsonl_internal_path_t next_path;
	next_path.parent = path;
	next_path.depth = path->depth + 1;
	next_path.path_component.container_type = source_value->type;

	size_t object_size = jsonl_round_size(sizeof(jsonl_value_t));
	switch (source_value->type)
	{
		case JSONL_TYPE_OBJECT:
		{
			size_t remove_value_count = 0;
			size_t add_value_count = 0;
			for (size_t i = 0; i != set_value_count; ++i)
			{
				int value_path_match = jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + i);
				if (value_path_match && set_value_table[i].path_length > path->depth)
				{
					int set_value_in_child_value = 0;
					if (source_value->type == set_value_table[i].path[path->depth].container_type)
						for (size_t j = 0; !set_value_in_child_value && j != source_value->object.value_count; ++j)
							if (source_value->object.table[j].name_length == set_value_table[i].path[path->depth].name.length && jsonl_memory_compare(source_value->object.table[j].name, set_value_table[i].path[path->depth].name.value, source_value->object.table[j].name_length))
								set_value_in_child_value = 1;
					if (set_value_in_child_value)
					{
						if (!set_value_table[i].value)
						{
							if (set_value_table[i].path_length == path->depth + 1)
							{
								++remove_value_count;
							}
						}
					}
					else
					{
						if (set_value_table[i].value)
						{
							if (set_value_table[i].path_length == path->depth + 1)
								++add_value_count;
							else
							{
								int set_value_already_counted = 0;
								for (size_t j = 0; j != i; ++j)
									if (jsonl_internal_set_values_input_path_compare(set_value_table + j, set_value_table + i, path->depth + 1))
										set_value_already_counted = 1;
								if (!set_value_already_counted)
								{
									++add_value_count;
								}
							}
						}
					}
				}
			}

			JSONL_ASSERT(remove_value_count <= source_value->object.value_count);
			size_t child_value_count = (source_value->object.value_count - remove_value_count) + add_value_count;
			size_t object_table_size = jsonl_round_size(child_value_count * sizeof(*value_buffer->object.table));
			if (object_size <= value_buffer_size)
			{
				value_buffer->object.value_count = child_value_count;
				*(void**)&value_buffer->object.table = (void*)((uintptr_t)value_buffer + object_size);
			}
			for (size_t n = source_value->object.value_count - remove_value_count, c = 0, i = 0; c != n; ++i)
			{
				JSONL_ASSERT(i < source_value->object.value_count && c < n);
				int child_value_removed = 0;
				for (size_t j = 0; !child_value_removed && j != set_value_count; ++j)
					if (!set_value_table[j].value)
					{
						int value_path_match = (set_value_table[j].path_length == path->depth + 1) && jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + j);
						if (value_path_match && set_value_table[j].path[path->depth].container_type == JSONL_TYPE_OBJECT)
						{
							if (source_value->object.table[i].name_length == set_value_table[j].path[path->depth].name.length && jsonl_memory_compare(source_value->object.table[i].name, set_value_table[j].path[path->depth].name.value, source_value->object.table[i].name_length))
								child_value_removed = 1;
						}
					}
				if (!child_value_removed)
				{
					size_t name_size = jsonl_round_size(source_value->object.table[i].name_length + 1);
					if (object_size + object_table_size + name_size <= value_buffer_size)
					{
						value_buffer->object.table[c].name_length = source_value->object.table[i].name_length;
						value_buffer->object.table[c].name = (char*)((uintptr_t)value_buffer + object_size + object_table_size);
						jsonl_copy_memory((void*)((uintptr_t)value_buffer + object_size + object_table_size), source_value->object.table[i].name, source_value->object.table[i].name_length + 1);
					}
					object_table_size += name_size;
					++c;
				}
			}
			for (size_t c = 0, i = 0; c != add_value_count; ++i)
				if (set_value_table[i].value)
				{
					JSONL_ASSERT(i < set_value_count);
					int value_path_match = jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + i);
					if (value_path_match && set_value_table[i].path_length > path->depth)
					{
						int set_value_in_child_value = 0;
						JSONL_ASSERT(source_value->type == JSONL_TYPE_OBJECT);
						if (source_value->type == set_value_table[i].path[path->depth].container_type)
							for (size_t j = 0; !set_value_in_child_value && j != source_value->object.value_count; ++j)
								if (source_value->object.table[j].name_length == set_value_table[i].path[path->depth].name.length && jsonl_memory_compare(source_value->object.table[j].name, set_value_table[i].path[path->depth].name.value, source_value->object.table[j].name_length))
									set_value_in_child_value = 1;
						if (!set_value_in_child_value)
						{
							JSONL_ASSERT(set_value_table[i].path_length > path->depth);
							JSONL_ASSERT(set_value_table[i].path[path->depth].container_type == JSONL_TYPE_OBJECT);
							int set_value_already_counted = 0;
							for (size_t j = 0; j != i; ++j)
								if (jsonl_internal_set_values_input_path_compare(set_value_table + j, set_value_table + i, path->depth + 1))
									set_value_already_counted = 1;
							if (!set_value_already_counted)
							{
								size_t name_size = jsonl_round_size(set_value_table[i].path[path->depth].name.length + 1);
								if (object_size + object_table_size + name_size <= value_buffer_size)
								{
									value_buffer->object.table[(source_value->object.value_count - remove_value_count) + c].name_length = set_value_table[i].path[path->depth].name.length;
									value_buffer->object.table[(source_value->object.value_count - remove_value_count) + c].name = (char*)((uintptr_t)value_buffer + object_size + object_table_size);
									jsonl_copy_memory((void*)((uintptr_t)value_buffer + object_size + object_table_size), set_value_table[i].path[path->depth].name.value, set_value_table[i].path[path->depth].name.length + 1);
									value_buffer->object.table[(source_value->object.value_count - remove_value_count) + c].value = 0;// debug init
								}
								object_table_size += name_size;
								++c;
							}
						}
					}
				}
			object_size += object_table_size;
			for (size_t n = source_value->object.value_count - remove_value_count, c = 0, i = 0; c != n; ++i)
			{
				JSONL_ASSERT(i < source_value->object.value_count && c < n);
				int child_value_removed = 0;
				for (size_t j = 0; !child_value_removed && j != set_value_count; ++j)
					if (!set_value_table[j].value)
					{
						int value_path_match = (set_value_table[j].path_length == path->depth + 1) && jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + i);
						if (value_path_match && set_value_table[j].path[path->depth].container_type == JSONL_TYPE_OBJECT)
						{
							if (source_value->object.table[i].name_length == set_value_table[j].path[path->depth].name.length && jsonl_memory_compare(source_value->object.table[i].name, set_value_table[j].path[path->depth].name.value, source_value->object.table[i].name_length))
								child_value_removed = 1;
						}
					}
				if (!child_value_removed)
				{
					JSONL_ASSERT(next_path.path_component.container_type == JSONL_TYPE_OBJECT);
					next_path.path_component.name.length = source_value->object.table[i].name_length;
					next_path.path_component.name.value = source_value->object.table[i].name;
					next_path.value_type = source_value->object.table[i].value->type;
					next_path.source_value = source_value->object.table[i].value;
					size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
					if (remaining_buffer_size)
						value_buffer->object.table[c].value = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
					size_t child_value_size = jsonl_internal_set_values(set_value_count, set_value_table, &next_path, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
					if (!child_value_size)
						return 0;
					object_size += child_value_size;
					++c;
				}
			}
			for (size_t c = 0, i = 0; c != add_value_count; ++i)
				if (set_value_table[i].value)
				{
					JSONL_ASSERT(i < set_value_count);
					int value_path_match = jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + i);
					if (value_path_match)
					{
						int set_value_in_child_value = 0;
						if (source_value->type == set_value_table[i].path[path->depth].container_type)
							for (size_t j = 0; !set_value_in_child_value && j != source_value->object.value_count; ++j)
								if (source_value->object.table[j].name_length == set_value_table[i].path[path->depth].name.length && jsonl_memory_compare(source_value->object.table[j].name, set_value_table[i].path[path->depth].name.value, source_value->object.table[j].name_length))
									set_value_in_child_value = 1;
						if (!set_value_in_child_value)
						{
							if (set_value_table[i].path_length == path->depth + 1)
							{
								size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
								if (remaining_buffer_size)
									value_buffer->object.table[(source_value->object.value_count - remove_value_count) + c].value = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
								size_t child_value_size = jsonl_internal_copy_value(set_value_table[i].value, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
								if (!child_value_size)
									return 0;
								object_size += child_value_size;
								++c;
							}
							else
							{
								int set_value_already_counted = 0;
								for (size_t j = 0; j != i; ++j)
									if (jsonl_internal_set_values_input_path_compare(set_value_table + j, set_value_table + i, path->depth + 1))
										set_value_already_counted = 1;
								if (!set_value_already_counted)
								{
									size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
									if (remaining_buffer_size)
										value_buffer->object.table[(source_value->object.value_count - remove_value_count) + c].value = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
									size_t child_value_size = jsonl_internal_set_values_create_path(i, set_value_count, set_value_table, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size), path->depth + 1, value_buffer);
									if (!child_value_size)
										return 0;
									object_size += child_value_size;
									++c;
								}
							}
						}
					}
				}
			break;
		}
		case JSONL_TYPE_ARRAY:
		{
			size_t remove_child_value_count = 0;
			size_t set_child_value_count = 0;
			size_t append_child_value_count = 0;
			size_t set_child_value_highest_index = (size_t)~0;
			for (size_t i = 0; i != set_value_count; ++i)
			{
				int value_path_match = jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + i);
				if (value_path_match && set_value_table[i].path_length > path->depth)
				{
					if (set_value_table[i].path[path->depth].index != (size_t)~0)
					{
						if (set_value_table[i].path_length == path->depth + 1)
						{
							if (set_value_table[i].value)
							{
								if (set_child_value_highest_index == (size_t)~0 || set_value_table[i].path[path->depth].index > set_child_value_highest_index)
									set_child_value_highest_index = set_value_table[i].path[path->depth].index;
								++set_child_value_count;
							}
							else if (set_value_table[i].path[path->depth].index < source_value->array.value_count)
								++remove_child_value_count;
						}
						else
						{
							if (set_value_table[i].value)
							{
								int set_value_already_counted = 0;
								for (size_t j = 0; !set_value_already_counted && j != i; ++j)
									if (set_value_table[j].path_length == set_value_table[i].path_length &&
										jsonl_internal_set_values_input_path_compare(set_value_table + j, set_value_table + i, path->depth) &&
										set_value_table[j].path[path->depth].container_type == JSONL_TYPE_ARRAY &&
										set_value_table[j].path[path->depth].index == set_value_table[i].path[path->depth].index)
										set_value_already_counted = 1;
								if (!set_value_already_counted)
								{
									if (set_child_value_highest_index == (size_t)~0 || set_value_table[i].path[path->depth].index > set_child_value_highest_index)
										set_child_value_highest_index = set_value_table[i].path[path->depth].index;
									++set_child_value_count;
								}
							}
						}
					}
					else
					{
						if (set_value_table[i].value)
							++append_child_value_count;
					}
				}
			}
			JSONL_ASSERT(remove_child_value_count <= source_value->array.value_count);
			JSONL_ASSERT(set_child_value_count <= set_value_count);
			JSONL_ASSERT(append_child_value_count <= set_value_count);

			size_t child_value_count = source_value->array.value_count - remove_child_value_count;
			if (set_child_value_highest_index != (size_t)~0 && child_value_count < set_child_value_highest_index + 1)
				child_value_count = set_child_value_highest_index + 1;
			child_value_count += append_child_value_count;

			size_t array_table_size = jsonl_round_size(child_value_count * sizeof(*value_buffer->array.table));
			if (object_size + array_table_size <= value_buffer_size)
			{
				value_buffer->array.value_count = child_value_count;
				*(void**)&value_buffer->array.table = (void*)((uintptr_t)value_buffer + object_size);
			}
			object_size += array_table_size;
			for (size_t erased = 0, i = 0; i != (child_value_count - append_child_value_count); ++i)
			{
				size_t overwrite_set_value_index = (size_t)~0;
				for (size_t set_value_index = 0; overwrite_set_value_index == (size_t)~0 && set_value_index != set_value_count; ++set_value_index)
					if (set_value_table[set_value_index].value)
					{
						int value_path_match = jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + set_value_index);
						if (value_path_match)
						{
							if (source_value->type == set_value_table[set_value_index].path[path->depth].container_type)
							{
								if (set_value_table[set_value_index].path[path->depth].index == i)
								{

									overwrite_set_value_index = set_value_index;
								}
							}
						}
					}
				if (overwrite_set_value_index == (size_t)~0)
				{
					for (int continue_erase = 1; continue_erase && i + erased < source_value->array.value_count;)
					{
						continue_erase = 0;
						for (size_t set_value_index = 0; !continue_erase && set_value_index != set_value_count; ++set_value_index)
							if (!set_value_table[set_value_index].value)
							{
								int value_path_match = jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + set_value_index);
								if (value_path_match)
								{
									if (source_value->type == set_value_table[set_value_index].path[path->depth].container_type)
									{
										if (set_value_table[set_value_index].path[path->depth].index == i + erased)
										{
											++erased;
											continue_erase = 1;
										}
									}
								}
							}
					}

					if (i + erased < source_value->array.value_count)
					{
						next_path.path_component.index = i;
						next_path.value_type = source_value->array.table[i + erased]->type;
						next_path.source_value = source_value->array.table[i + erased];
						size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
						if (remaining_buffer_size)
							value_buffer->array.table[i] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
						size_t child_value_size = jsonl_internal_set_values(set_value_count, set_value_table, &next_path, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
						if (!child_value_size)
							return 0;
						object_size += child_value_size;
					}
					else
					{
						size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
						if (remaining_buffer_size)
							value_buffer->array.table[i] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
						size_t child_value_size = jsonl_internal_create_null_value(value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
						if (!child_value_size)
							return 0;
						object_size += child_value_size;
					}
				}
				else
				{
					JSONL_ASSERT(set_value_table[overwrite_set_value_index].path[path->depth].index != (size_t)~0);
					JSONL_ASSERT(set_value_table[overwrite_set_value_index].value);

					if (set_value_table[overwrite_set_value_index].path_length > path->depth + 1)
					{
						if (i + erased < source_value->array.value_count)
						{
							next_path.path_component.index = i;
							next_path.value_type = source_value->array.table[i + erased]->type;
							next_path.source_value = source_value->array.table[i + erased];
							size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
							if (remaining_buffer_size)
								value_buffer->array.table[i] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
							size_t child_value_size = jsonl_internal_set_values(set_value_count, set_value_table, &next_path, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
							if (!child_value_size)
								return 0;
							object_size += child_value_size;
						}
						else
						{
							size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
							if (remaining_buffer_size)
								value_buffer->array.table[i] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
							size_t child_value_size = jsonl_internal_set_values_create_path(overwrite_set_value_index, set_value_count, set_value_table, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size), path->depth + 1, value_buffer);
							if (!child_value_size)
								return 0;
							object_size += child_value_size;
						}
					}
					else
					{
						size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
						if (remaining_buffer_size)
							value_buffer->array.table[i] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
						size_t child_value_size = jsonl_internal_copy_value(set_value_table[overwrite_set_value_index].value, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
						if (!child_value_size)
							return 0;
						object_size += child_value_size;
					}
				}
			}
			for (size_t append_set_value_index_offset = 0, c = 0, i = 0; c != append_child_value_count; ++i)
			{
				size_t append_set_value_index = (size_t)~0;
				for (size_t set_value_index = append_set_value_index_offset; append_set_value_index == (size_t)~0 && set_value_index != set_value_count; ++set_value_index)
					if (set_value_table[set_value_index].value)
					{
						int value_path_match = jsonl_internal_set_values_is_set_value_in_path(path, set_value_table + set_value_index);
						if (value_path_match)
						{
							if (source_value->type == set_value_table[set_value_index].path[path->depth].container_type)
							{
								if (set_value_table[set_value_index].path[path->depth].index == (size_t)~0)
								{
									append_set_value_index = set_value_index;
									append_set_value_index_offset = set_value_index + 1;
								}
							}
						}
					}
				if (append_set_value_index != (size_t)~0)
				{
					if (set_value_table[append_set_value_index].path_length > path->depth + 1)
					{
						size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
						if (remaining_buffer_size)
							value_buffer->array.table[(child_value_count - append_child_value_count) + c] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
						size_t child_value_size = jsonl_internal_set_values_create_path(append_set_value_index, set_value_count, set_value_table, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size), path->depth + 1, value_buffer);
						if (!child_value_size)
							return 0;
						object_size += child_value_size;
					}
					else
					{
						next_path.path_component.index = (child_value_count - append_child_value_count) + c;
						next_path.value_type = set_value_table[append_set_value_index].value->type;
						next_path.source_value = set_value_table[append_set_value_index].value;
						size_t remaining_buffer_size = (object_size <= value_buffer_size) ? (value_buffer_size - object_size) : 0;
						if (remaining_buffer_size)
							value_buffer->array.table[(child_value_count - append_child_value_count) + c] = (jsonl_value_t*)((uintptr_t)value_buffer + object_size);
						size_t child_value_size = jsonl_internal_copy_value(set_value_table[append_set_value_index].value, value_buffer, remaining_buffer_size, (jsonl_value_t*)((uintptr_t)value_buffer + object_size));
						if (!child_value_size)
							return 0;
						object_size += child_value_size;
					}
					++c;
				}
			}
			break;
		}
		case JSONL_TYPE_STRING:
		{
			size_t data_size = jsonl_round_size(source_value->string.length + 1);
			if (object_size + data_size <= value_buffer_size)
			{
				value_buffer->string.length = source_value->string.length;
				value_buffer->string.value = (char*)((uintptr_t)value_buffer + object_size);
				jsonl_copy_memory((char*)((uintptr_t)value_buffer + object_size), source_value->string.value, source_value->string.length + 1);
			}
			object_size += data_size;
			break;
		}
		case JSONL_TYPE_NUMBER:
		{
			if (object_size <= value_buffer_size)
			{
#ifdef JSONL_FIXED_POINT_NUMBER_FORMAT
				value_buffer->number.sign = source_value->number.sign;
				value_buffer->number.integer = source_value->number.integer;
				value_buffer->number.fraction = source_value->number.fraction;
#else
				value_buffer->number.value = source_value->number.value;
#endif
			}
			break;
		}
		case JSONL_TYPE_BOOLEAN:
		{
			if (object_size <= value_buffer_size)
				value_buffer->boolean.value = source_value->boolean.value;
			break;
		}
		case JSONL_TYPE_NULL:
		{
			break;
		}
		default:
			return 0;
	}
	if (object_size <= value_buffer_size)
	{
		value_buffer->size = object_size;
		value_buffer->parent = (jsonl_value_t*)parent;
		value_buffer->type = source_value->type;
	}
	return object_size;
}

size_t jsonl_set_values(const jsonl_value_t* value_tree, size_t set_value_count, const jsonl_set_value_t* set_value_table, size_t value_buffer_size, jsonl_value_t* value_buffer)
{
	for (size_t i = 0; i != set_value_count; ++i)
		if (!set_value_table[i].path_length && !set_value_table[i].value)
			return 0;

	jsonl_internal_path_t root_path;
	root_path.parent = 0;
	root_path.depth = 0;
	root_path.path_component.container_type = 0;
	root_path.value_type = value_tree->type;
	root_path.source_value = value_tree;

	return jsonl_internal_set_values(set_value_count, set_value_table, &root_path, 0, value_buffer_size, value_buffer);
}

#ifdef __cplusplus
}
#endif