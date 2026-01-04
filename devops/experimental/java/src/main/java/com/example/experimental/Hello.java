package com.example.experimental;

public class Hello {
    public static final String HELLO = "hello world";

    public static String extractWorld(String greeting) {
        if (greeting == null) {
            return null;
        }
        int index = greeting.indexOf("world");
        return index >= 0 ? greeting.substring(index, index + "world".length()) : null;
    }
}
