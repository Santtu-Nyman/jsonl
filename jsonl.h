/*
	JSON library version 3.0.0 2020-11-07 by Santtu Nyman.
	git repository https://github.com/Santtu-Nyman/jsonl

	Description
		Cross-Platform C library without dependencies for parsing and modifying JSON text.
		This library does not use any OS services or runtime libraries.
		Everything done by this library is purely computational,
		so there is no need for C standard library or any OS.

		Documentation of functions and data structures of the library are provided in the file "jsonl.h".

		The library was originally written only for parsing JSON files.
		It was originally used in OAMK storage robot project of class TVT17SPL in 2019
		https://blogi.oamk.fi/2019/12/28/projektiryhmien-yhteistyo-kannatti-varastorobo-jarjestelma-ohjaa-lastaa-kuljettaa-ja-valvoo/
		The purpose of the library in that project was to load program configuration
		for the central server used to receive user request and command the robots.

		In 2020 this library was expanded to first print the parsed JSON data back to JSON text and
		at the late 2020 functionality for modifying JSON data was added. At this point the library
		was significantly different from what it originally was and I decided to rename the library from
		JSON parser library (jsonpl) to just JSON library (jsonl), because the original name was conflicting
		with it's expanded purpose.

	Version history
		version 3.0.0 2020-11-08
			Added new functionality for modifying JSON data.
			Changed jsonpl_get_value function to searching from paths.
			Improved old documentation.
			Added history information to library description.
			Renamed the library and moved away from old repository https://github.com/Santtu-Nyman/jsonpl
		version 2.9.0 2020-08-01
			Added new functionality for printing the parsed JSON content to JSON text.
			This is done using the new jsonpl_print function.
		version 2.0.0 2020-04-12
			Changed data structure "jsonpl_value_t" of the library API.
			This API change was very minor, but required for new features in
			future and syntax of the new API is more consistent.
			Added alternative fixed point number representation.
			Clarified documentation in the file "jsonpl.h".
		version 1.1.0 2020-01-28
			Added license information and funtion for searching parsed json.
		version 1.0.0 2019-10-31
			First publicly available version.
			
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

#ifndef JSONL_H
#define JSONL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define JSONL_TYPE_ERROR   0
#define JSONL_TYPE_OBJECT  1
#define JSONL_TYPE_ARRAY   2
#define JSONL_TYPE_STRING  3
#define JSONL_TYPE_NUMBER  4
#define JSONL_TYPE_BOOLEAN 5
#define JSONL_TYPE_NULL    6

#ifdef JSONL_FIXED_POINT_NUMBER_FORMAT
typedef struct jsonl_number_value_t
{
	int sign;
	uint64_t integer;
	uint64_t fraction;
} jsonl_number_value_t;
#else
typedef struct jsonl_number_value_t
{
	double value;
} jsonl_number_value_t;
#endif

typedef struct jsonl_value_t
{
	size_t size;
	struct jsonl_value_t* parent;
	int type;
	union
	{
		struct
		{
			size_t value_count;
			struct jsonl_value_t** table;
		} array;
		struct jsonl_object_t
		{
			size_t value_count;
			struct
			{
				size_t name_length;
				char* name;
				struct jsonl_value_t* value;
			}* table;
		} object;
		struct
		{
			size_t length;
			char* value;
		} string;
		jsonl_number_value_t number;
		struct
		{
			int value;
		} boolean;
	};
} jsonl_value_t;
/*
	Structure
		jsonl_value_t

	Description
		The jsonl_value_t structure defines format of value tree that represents the contents of the JSON text.
		One jsonl_value_t structure is used to represent single JSON value.

	Members
		size
			This member specifies size of this structure in bytes.
			The size includes contents pointed by member variables and sizes of child values.

		type
			This member specifies the type of this value.
			This member can be one of the following constants.

				JSON_TYPE_OBJECT
					The specified value is an object.

				JSON_TYPE_ARRAY
					The specified value is an array.

				JSON_TYPE_STRING
					The specified value is a string.

				JSON_TYPE_NUMBER
					The specified value is a number.

				JSON_TYPE_BOOLEAN
					The specified value is a boolean.

			The constant JSON_TYPE_ERROR is not used. It is for internal uses of this library only.

		array.value_count
			Value of this member is only valid if type of this value is an array.
			This variable specifies number of values in this array.

		array.table
			Value of this member is only valid if type of this value is an array.
			A pointer to beginning of an array of pointers to child values.

		object.value_count
			Value of this member is only valid if type of this value is an object.
			This variable specifies number of values in this object.

		object.table
			Value of this member is only valid if type of this value is an object.
			A pointer to beginning of an array of pointers to child values.
			Objects pointed in this array are structures of tree members that represents name value pairs.
			The member name_length specifies size of this string in bytes not including null terminating character or padding bytes.
			The member name specifies beginning address of null terminated UTF-8 string that specifies name in a name value pair.
			The member value specifies address of a child value.

		string.length
			Value of this member is only valid if type of this value is a string.
			The size of this string in bytes not including null terminating character or padding bytes.

		string.value
			Value of this member is only valid if type of this value is a string.
			A pointer to beginning of this string.
			The format of the string is null terminated UTF-8.

		number.value
			Value of this member is only valid if type of this value is a number.
			This variable specifies value of this number.
			This member is not defined, if jsonl is compiled with JSONL_FIXED_POINT_NUMBER_FORMAT defined.
			This member variable is the default number representation. It uses the IEEE 754-2008 binary64 format.
			It is recommended by the JSON RFC for good interoperability, but this library provides alternative format for representing numbers.
			The alternative format is 128 bit unsigned fixed point number that is divided half for integer and fraction parts and separate sign.
			To use the alternative number format compile with JSONL_FIXED_POINT_NUMBER_FORMAT defined.

		number.sign
			This value is only defined, if jsonl is compiled with JSONL_FIXED_POINT_NUMBER_FORMAT defined.
			Value of this member is only valid if type of this value is a number.
			This variable specifies sign part of this number.
			The sign is one, if the number is negative otherwise the sign is zero.

		number.integer
			This value is only defined, if jsonl is compiled with JSONL_FIXED_POINT_NUMBER_FORMAT defined.
			Value of this member is only valid if type of this value is a number.
			This variable specifies 64 bit integer part of this number.

		number.fraction
			This value is only defined, if jsonl is compiled with JSONL_FIXED_POINT_NUMBER_FORMAT defined.
			Value of this member is only valid if type of this value is a number.
			This variable specifies 64 bit fraction part of this number.

		boolean.value
			Value of this member is only valid if type of this value is a boolean.
			This variable specifies value of this boolean.
			This variable is always either zero or one.
*/

size_t jsonl_parse_text(size_t json_text_size, const char* json_text, size_t value_buffer_size, jsonl_value_t* value_buffer);
/*
	Function
		jsonl_parse_text

	Description
		The json_parse_text function converts a JSON text to tree structure that represents the contents of the JSON text.
		The parser function unescapes strings and decodes numbers.

	Parameters
		json_text_size
			This parameter specifies the size of JSON text in bytes.
			Characters of JSON text are UTF-8 encoded allowing characters to take multiple bytes.
		json_text
			This parameter is a pointer to buffer that contains the JSON text.
			The text may be optionally null terminated.
		value_buffer_size
			This parameter specifies the size of buffer pointed by parameter value_buffer in bytes.

			If the buffer is not large enough to hold the tree data, the function returns required buffer size in bytes.
			Required size of the buffer can't be zero for any JSON text.
		value_buffer
			This parameter is a pointer to a buffer that receives the value tree if size of the buffer is sufficiently large.
			The whole tree is written to this buffer and will not contain any pointers to any memory outside of the value tree buffer.

			All strings in the value tree are null terminated and aligned to size of pointer.

			If the buffer is not large enough to hold the tree data, the contents of the tree value buffer are undefined.

			If the buffer size is zero, this parameter is ignored.
	Return
		If the JSON text is successfully parsed, the return value is size of value three in bytes and zero otherwise.

		If the returned size is not zero and not greater than the size of the value tree buffer,
		the buffer will contain value tree representing the contents of the JSON text.
*/

size_t jsonl_print(const jsonl_value_t* value_tree, size_t json_text_buffer_size, char* json_text_buffer);
/*
	Function
		jsonl_print

	Description
		This function prints contents of given JSON value to buffer in JSON text format.

	Parameters
		value_tree
			This parameter is a pointer to JSON value to be printed.

			This value can be any valid type of JSON value.
		json_text_buffer_size
			This parameter specifies the size of buffer pointed by parameter json_text_buffer in bytes.

			If the buffer is not large enough to hold the resulting JSON text, the function returns required buffer size in bytes.
			Required size of the buffer can't be zero for any JSON text.
		json_text_buffer
			This parameter is a pointer to buffer, that will receive the resulting JSON text in UTF-8 format.

			If the buffer is not large enough to hold the tree data, the contents of the tree value buffer are undefined.

			If the buffer size is zero, this parameter is ignored.

	Return
		If the JSON text is successfully printed, the return value is size of JSON text in bytes and zero otherwise.
		The printed JSON text is non null terminated.
*/

typedef struct jsonl_path_component_t
{
	int container_type;
	union
	{
		size_t index;
		struct
		{
			size_t length;
			char* value;
		} name;
	};
} jsonl_path_component_t;
/*
	Structure
		jsonl_path_component_t

	Description
		The jsonl_path_component_t structure defines format of path component in a tree of JSON data.
		These components can be value names in an object or indices in an array.
		Array of jsonl_path_component_t structures can be used to create a path to any value in JSON data.

	Members
		container_type
			This variable specifies the type of this path component.
			This member can be one of the following constants. Other types are not allowed.

				JSON_TYPE_OBJECT
					The specified path is in an object.

				JSON_TYPE_ARRAY
					The specified path is in an array.

		index
			Value of this member is only valid if type of this path component is array.
			This is true when, value of container_type member is JSON_TYPE_ARRAY.
			This variable specifies the array index of this path component.
			If this member is set to (size_t)~0, this component will be appended to the end off the array.
			If this member is (size_t)~0 when removing a value, nothing will be removed.

		name.length
			Value of this member is only valid if type of this path component is object.
			This is true when, value of container_type member is JSON_TYPE_OBJECT.
			This member specifies the size of this path component name in bytes not including null terminating character.

		name.value
			Value of this member is only valid if type of this path component is object.
			This is true when, value of container_type member is JSON_TYPE_OBJECT.
			This member is a pointer to beginning of the path component name.
			The length of this string is specified by name.length member.
			The format of the string is null terminated UTF-8.
*/

typedef struct jsonl_set_value_t
{
	size_t path_length;
	const jsonl_path_component_t* path;
	const jsonl_value_t* value;
} jsonl_set_value_t;
/*
	Structure
		jsonl_set_value_t

	Description
		The jsonl_set_value_t structure defines format of single modification to JSON tree.
		This modification can be writing specific JSON value, appending to an array or removing specific JSON value.

		To write specific JSON value, set member value to address of new value.
		The specified location will receive copy of value pointed by member value.
		If this value already exists it will be overwritten.
		If path to specified location does not exits it will be created.

		To append JSON value to an array set index in the target array to (size_t)~0.
		Otherwise appending works identically to writing specific JSON value.
		If the target array does not exist it will also be created.

		To remove specific JSON value, set set member value zero.
		The JSON value in specified location will be removed if it exists.
		If array index (size_t)~0 is used in any part of the path nothing will be removed.
		This is because index (size_t)~0 refers to location in array past all exiting values.

	Members
		path_length
			This member specifies length of the path in JSON data tree in number of components.

		path
			This member is a pointer to beginning of table components.
			The first component in the table is located in the root of JSON tree.
			Number of component in this table is specified by member path_length.

		value
			This member is a pointer to JSON value that will be copied to specified location.
			This member can be set to zero for removing a value.
*/

const jsonl_value_t* jsonl_get_value(const jsonl_value_t* parent_value, size_t path_length, const jsonl_path_component_t* path_table, int required_value_type);
/*
	Function
		jsonl_get_value

	Description
		This function gets address of a child value from given JSON value.
		
	Parameters
		parent_value
			This parameter is a pointer to parent JSON object.
			This function returns zero if type of this JSON value is not object.
		path_length
			Length of the path to child value.
		path_table
			Table of path components for specifying the path to child value.
			The length of the path is specified by path_length parameter.
		required_value_type
			This parameter specifies required type for the child value.
			If type of the child value is not type specified by this parameter, this function returns zero.
			If this parameter is zero. It is ignored and type of the child value is not checked.

	Return
		The function returns address of child value from given JSON value when specified child value is found, otherwise zero is returned.
*/

size_t jsonl_set_values(const jsonl_value_t* value_tree, size_t set_value_count, const jsonl_set_value_t* set_value_table, size_t value_buffer_size, jsonl_value_t* value_buffer);
/*
	Function
		jsonl_set_values

	Description
		This function makes list of modifications to JSON tree.
		The modifications are given as a table of jsonl_set_value_t structures.
		Everyone of these structures specifies single modification.

		None of the modifications must be overlapping.
		The result of this function is undefined if any of the modification overlap.

	Parameters
		value_tree
			This parameter is a pointer to JSON tree to be modified.
			The JSON value pointed by this parameter is not modified, the modifications will be made to copy created from it.
		set_value_count
			This parameter specifies number of modifications to make.
		set_value_table
			This parameter is a pointer to table of structures to specify the modifications to make.

			In these structures parent and size members are ignored from the structure pointed by member value.
		value_buffer_size
			This parameter specifies the size of buffer pointed by parameter value_buffer in bytes.

			If the buffer is not large enough to hold the new modified tree data, the function returns required buffer size in bytes.
			Required size of the buffer can't be zero for the new JSON tree.
		value_buffer
			This parameter is a pointer to a buffer that receives the new JSON tree if size of the buffer is sufficiently large.
			The whole tree is written to this buffer and will not contain any pointers to any memory outside of the value tree buffer.

			All strings in the value tree are null terminated and aligned to size of pointer.

			If the buffer is not large enough to hold the tree data, the contents of the tree value buffer are undefined.

			If the buffer size is zero, this parameter is ignored.

	Return
		If the JSON tree is successfully modified, the return value is size of value three in bytes and zero otherwise.

		If the returned size is not zero and not greater than the size of the value tree buffer,
		the buffer will contain new modified JSON tree.
*/

#ifdef __cplusplus
}
#endif

#endif