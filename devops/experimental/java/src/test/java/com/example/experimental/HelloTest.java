package com.example.experimental;

import static org.junit.jupiter.api.Assertions.*;

import org.junit.jupiter.api.Test;

class HelloTest {
    @Test
    void findsWorld() {
        assertEquals("world", Hello.extractWorld(Hello.HELLO));
    }

    @Test
    void returnsNullWhenMissing() {
        assertNull(Hello.extractWorld("no match"));
    }
}
