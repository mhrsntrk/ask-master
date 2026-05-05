package truncate

import (
	"testing"
	"strings"
)

func TestTruncate_EmptyString(t *testing.T) {
	s := ""
	maxLen := 10
	expected := ""
	if got := String(s, maxLen); got != expected {
		t.Errorf("String(%q, %d) = %q; want %q", s, maxLen, got, expected)
	}
}

func TestTruncate_ExactBoundary(t *testing.T) {
	s := strings.Repeat("a", 120)
	maxLen := 120
	expected := s
	if got := String(s, maxLen); got != expected {
		t.Errorf("String(120 chars, 120) = %q; want %q", got, expected)
	}
}

func TestTruncate_MultibyteRunes(t *testing.T) {
	s := "日本語日本語日本語"
	maxLen := 3
	expected := "日本語"
	if got := String(s, maxLen); got != expected {
		t.Errorf("String(%q, %d) = %q; want %q", s, maxLen, got, expected)
	}
}

func TestTruncate_ShortString(t *testing.T) {
	s := "hi"
	maxLen := 10
	expected := "hi"
	if got := String(s, maxLen); got != expected {
		t.Errorf("String(%q, %d) = %q; want %q", s, maxLen, got, expected)
	}
}

func TestTruncateWithEllipsis_Truncated(t *testing.T) {
	s := "hello world"
	maxLen := 8
	expected := "hello..." // 5 chars + 3 ellipsis = 8
	if got := StringWithEllipsis(s, maxLen); got != expected {
		t.Errorf("StringWithEllipsis(%q, %d) = %q; want %q", s, maxLen, got, expected)
	}
}

func TestTruncateWithEllipsis_NotTruncated(t *testing.T) {
	s := "hi"
	maxLen := 10
	expected := "hi"
	if got := StringWithEllipsis(s, maxLen); got != expected {
		t.Errorf("StringWithEllipsis(%q, %d) = %q; want %q", s, maxLen, got, expected)
	}
}
