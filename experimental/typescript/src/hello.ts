export const HELLO = "hello world";

export function extractWorld(greeting?: string): string | null {
  if (typeof greeting !== "string") {
    return null;
  }
  const needle = "world";
  const idx = greeting.indexOf(needle);
  if (idx === -1) {
    return null;
  }
  return greeting.slice(idx, idx + needle.length);
}
