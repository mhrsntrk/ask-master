package truncate

import (
	"unicode/utf8"
)

func String(s string, maxLen int) string {
	if maxLen <= 0 {
		return ""
	}

	if utf8.RuneCountInString(s) <= maxLen {
		return s
	}

	var runeCount int
	for i := range s {
		if runeCount == maxLen {
			return s[:i]
		}
		runeCount++
	}

	return s
}

func StringWithEllipsis(s string, maxLen int) string {
	if maxLen <= 3 {
		return String(s, maxLen)
	}

	if utf8.RuneCountInString(s) <= maxLen {
		return s
	}

	return String(s, maxLen-3) + "..."
}
