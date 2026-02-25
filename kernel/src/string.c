#include <string.h>
#include <stdint.h>
#include <stddef.h>

// Copies n bytes from memory area src to memory area dest
void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;
    
    // Copy byte by byte; assumes memory regions do not overlap
    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
    return dest;
}

// Fills the first n bytes of the memory area s with the constant byte c
void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    
    // Set each byte in the range to the provided value
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

// Copies n bytes from memory area src to memory area dest, handling overlaps
void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
    
    // If source is after destination, copy forward
    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } 
    // If source is before destination, copy backward to avoid overwriting data
    else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }
    return dest;
}

// Compares the first n bytes of memory areas s1 and s2
int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            // Return difference if a mismatch is found
            return p1[i] < p2[i] ? -1 : 1;
        }
    }
    // Areas are identical
    return 0;
}

// Compares up to n characters of two strings
int strncmp(const char *s1, const char *s2, size_t n) {
    // Iterate while characters match and null terminator is not reached
    while (n > 0 && *s1 != '\0' && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    // If limit n is reached, strings are considered equal up to that point
    if (n == 0) {
        return 0;
    }
    // Return the difference between the first non-matching characters
    return (*(unsigned char *)s1 - *(unsigned char *)s2);
}

// Compares two null-terminated strings
int strcmp(const char *s1, const char *s2) {
    // Continue until characters differ or null terminator is found
    while (*s1 != '\0' && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    // Return the difference; 0 indicates equality
    return (*(unsigned char *)s1 - *(unsigned char *)s2);
}

// Calculates the length of a null-terminated string
size_t strlen(const char *s) {
    size_t len = 0;
    // Count characters until the null terminator
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

// Converts an unsigned 32-bit integer into a decimal string representation
void int_to_str(uint32_t value, char *buffer) {
    // Handle the zero case explicitly
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    char temp[16]; // Temporary storage for digits in reverse order
    int i = 0;
    
    // Extract digits by taking the remainder of 10
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Reverse the temporary string into the provided buffer
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    // Null-terminate the final string
    buffer[j] = '\0';
}