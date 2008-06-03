#pragma once

#include <fileref.h>
#include <string>

//  Indicates the users preference rating of an item on a scale of 0-99 (0 = unrated, 1-12 = One Star, 
//  13-37 = Two Stars, 38-62 = Three Stars, 63-87 = Four Stars, 88-99 = Five Stars).
unsigned char rating(const TagLib::FileRef &fileref);
std::wstring albumArtist(const TagLib::FileRef &fileref);
