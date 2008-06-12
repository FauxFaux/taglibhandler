#pragma once

#include <fileref.h>
#include <string>
#include <vector>
#include <windows.h> // for SYSTEMTIME.

//  Indicates the users preference rating of an item on a scale of 0-99 (0 = unrated, 1-12 = One Star, 
//  13-37 = Two Stars, 38-62 = Three Stars, 63-87 = Four Stars, 88-99 = Five Stars).
unsigned char rating(const TagLib::FileRef &fileref);
std::wstring albumArtist(const TagLib::FileRef &fileref);
typedef std::vector<std::wstring> wstrvec_t;
wstrvec_t keywords(const TagLib::FileRef &fileref);

// Fill at least the year, and, if possible, the month and day.
// Order by completeness (ie. yyyy-mm-dd is better than yyyy), 
//  then by order of apperance in the tag. Pick the first.
SYSTEMTIME releasedate(const TagLib::FileRef &fileref);
std::wstring composer(const TagLib::FileRef &fileref);
std::wstring conductor(const TagLib::FileRef &fileref);
std::wstring subtitle(const TagLib::FileRef &fileref);
std::wstring label(const TagLib::FileRef &fileref);
std::wstring producer(const TagLib::FileRef &fileref);
std::wstring mood(const TagLib::FileRef &fileref);
