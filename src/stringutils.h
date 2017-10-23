#pragma once

#include <stdlib.h>

/**
 * @brief Get a hexadecimal string representation from memory buffer.
 * @param[in] src Buffer's pointer.
 * @param[in] sz Buffer's size.
 * @return A pointer to the result string. Caller is responsible for the result lifetime.
 */
static inline char* to_hex_string(const void* src, long unsigned int sz)
{
	static const char lookup[] =
	{
		'0', '1', '2', '3',
		'4', '5', '6', '7',
		'8', '9', 'a', 'b',
		'c', 'd', 'e', 'f'
	};

	const char* source;
	char* result;
	long unsigned int i;

	result = NULL;
	if (src && sz)
	{
		source = (const char*)src;
		result = (char*)malloc(sz * 2 + 1);
		if (result)
		{
			result[sz * 2] = 0;
			for (i = 0; i < sz; ++i)
			{
				result[i * 2] = lookup[(source[i] & 0xf0) >> 4];
				result[i * 2 + 1] = lookup[source[i] & 0x0f];
			}
		}
	}
	return result;
}
