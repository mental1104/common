package com.mental1104.common;

import java.lang.reflect.Array;
import java.util.Map;
import java.util.Objects;

public final class Contains {
  private Contains() {
  }

  public static boolean contains(Object haystack, Object needle) {
    if (haystack == null) {
      return false;
    }
    if (haystack instanceof CharSequence) {
      return containsCharSequence((CharSequence) haystack, needle);
    }
    if (haystack instanceof Map<?, ?>) {
      return ((Map<?, ?>) haystack).containsKey(needle);
    }
    if (haystack instanceof Iterable<?>) {
      return inIterableRaw((Iterable<?>) haystack, needle);
    }
    if (haystack.getClass().isArray()) {
      return inArrayRaw(haystack, needle);
    }
    return false;
  }

  public static boolean inString(CharSequence text, CharSequence sub) {
    if (text == null || sub == null) {
      return false;
    }
    return text.toString().contains(sub.toString());
  }

  public static boolean inChar(CharSequence text, char c) {
    if (text == null) {
      return false;
    }
    return text.toString().indexOf(c) >= 0;
  }

  public static <T> boolean inIterable(Iterable<T> items, T value) {
    if (items == null) {
      return false;
    }
    for (T item : items) {
      if (Objects.equals(item, value)) {
        return true;
      }
    }
    return false;
  }

  public static <T> boolean inArray(T[] items, T value) {
    if (items == null) {
      return false;
    }
    for (T item : items) {
      if (Objects.equals(item, value)) {
        return true;
      }
    }
    return false;
  }

  public static <K, V> boolean inMapKey(Map<K, V> map, K key) {
    return map != null && map.containsKey(key);
  }

  public static <K, V> boolean inMapValue(Map<K, V> map, V value) {
    return map != null && map.containsValue(value);
  }

  private static boolean containsCharSequence(CharSequence text, Object needle) {
    if (needle == null) {
      return false;
    }
    if (needle instanceof CharSequence) {
      return inString(text, (CharSequence) needle);
    }
    if (needle instanceof Character) {
      return inChar(text, (Character) needle);
    }
    return false;
  }

  private static boolean inIterableRaw(Iterable<?> items, Object needle) {
    for (Object item : items) {
      if (Objects.equals(item, needle)) {
        return true;
      }
    }
    return false;
  }

  private static boolean inArrayRaw(Object array, Object needle) {
    int len = Array.getLength(array);
    for (int i = 0; i < len; i++) {
      Object item = Array.get(array, i);
      if (Objects.equals(item, needle)) {
        return true;
      }
    }
    return false;
  }
}
