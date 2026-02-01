package com.mental1104.common;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.junit.jupiter.api.Test;

class ContainsTest {
  @Test
  void containsStringAndChar() {
    assertTrue(Contains.contains("hello", "ell"));
    assertTrue(Contains.contains("hello", 'e'));
    assertFalse(Contains.contains("hello", 'z'));
    assertFalse(Contains.contains("hello", 101));
    assertTrue(Contains.inString("hello", "ell"));
    assertTrue(Contains.inChar("hello", 'e'));
    assertFalse(Contains.inString("hello", "zz"));
  }

  @Test
  void containsIterableAndArray() {
    List<Integer> items = Arrays.asList(1, 2, 3);
    assertTrue(Contains.contains(items, 2));
    assertFalse(Contains.contains(items, 4));
    assertTrue(Contains.inIterable(items, 2));
    assertFalse(Contains.inIterable(items, 4));

    Integer[] arr = {1, 2, 3};
    assertTrue(Contains.inArray(arr, 2));
    assertFalse(Contains.inArray(arr, 4));
  }

  @Test
  void containsPrimitiveArray() {
    int[] nums = {1, 2, 3};
    assertTrue(Contains.contains(nums, 2));
    assertFalse(Contains.contains(nums, 4));
  }

  @Test
  void containsMapKeyAndValue() {
    Map<String, Integer> map = new HashMap<>();
    map.put("a", 1);
    assertTrue(Contains.contains(map, "a"));
    assertFalse(Contains.contains(map, "b"));
    assertTrue(Contains.inMapKey(map, "a"));
    assertFalse(Contains.inMapKey(map, "b"));
    assertTrue(Contains.inMapValue(map, 1));
    assertFalse(Contains.inMapValue(map, 2));
  }

  @Test
  void handlesNulls() {
    assertFalse(Contains.contains(null, "x"));
    List<String> items = new ArrayList<>();
    items.add(null);
    assertTrue(Contains.contains(items, null));
    assertFalse(Contains.contains("hello", null));
  }
}
