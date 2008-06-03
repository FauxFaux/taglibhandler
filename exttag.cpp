#include "exttag.h"
#include <boost/preprocessor.hpp>
//#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/compare.hpp>
#include <windows.h>
#include <propkey.h>

#include <asftag.h>
#include <apetag.h>
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <unknownframe.h>
#include <xiphcomment.h>
#include <mpegfile.h>

using namespace TagLib;

// === How this file works. ===
//  - Functions like uchar rating(TagLib::FileRef &) are exposed.
//  - These take the fileref's tag, and attempt to cast it to all of the TAG_CLASSES
//  - If an cast succeeds, it calls the worker function for the type (ie. readrating(APE::TAG..))
//  - If the tag is none of the tag types, it's probably a tag union:
//    - They will then attempt to cast the fileref's file to an MPEG file.
//    - A splitter is defined that will take all the possible tag types from this file, and upcast+return them.
//
// Basically, all of the useful functionality is rooted in the ie. readrating() functions, the rest is support.


// The types of tags to attempt to extract from a tag() type.
#define TAG_CLASSES (const Ogg::XiphComment)(const ASF::Tag)(const APE::Tag)(const ID3v2::Tag)

// Attempt to upcast var to type. If it succeeds, /return/ func(upcasted);
#define RETURN_IF_UPCAST(func, type, var) if (type *ty = dynamic_cast<type *>(var)) return func(ty);

// Generate a function to split an MPEG::File into it's respective tag types,
//  and attempt to call the upcasted versions.
// ID3v1 is not handled as this interface won't be reached for anything that it supports.
#define FILE_SPLITTER_MPEG(ret, func, def)                           \
	ret func(MPEG::File *file)                                       \
	{                                                                \
		RETURN_IF_UPCAST(func, const ID3v2::Tag, file->ID3v2Tag());  \
		RETURN_IF_UPCAST(func, const APE::Tag,   file->APETag()  );  \
		def;												         \
	}

// Inner body of the tag foreach loop; return_if_upcast all of the classes in order.
#define UPCAST_CALL_N_RETURN_TAG(r, data, elem) RETURN_IF_UPCAST(data, elem, tag)

// Generate the exposed functions and the stuff they require.
#define READER_FUNC(ret, name, def)                                               \
	FILE_SPLITTER_MPEG(ret, read##name, def);                                     \
	ret name(const TagLib::FileRef &fileref)                                      \
	{                                                                             \
		const TagLib::Tag *tag = fileref.tag();                                   \
		BOOST_PP_SEQ_FOR_EACH(UPCAST_CALL_N_RETURN_TAG, read##name, TAG_CLASSES); \
		                                                                          \
		TagLib::File *file = fileref.file();                                      \
		RETURN_IF_UPCAST(read##name, MPEG::File, file);                           \
		                                                                          \
		def;                                                                      \
	}


const String emptyString;

bool is_iequal(const std::wstring& l, const std::wstring& r)
{
	if (l == r)
		return true;
	if (l.size() != r.size())
		return false;

	std::wstring::const_iterator rit = r.begin();
	for (std::wstring::const_iterator it = l.begin(); it != l.end(); ++it)
		if (tolower(*it) != tolower(*rit++))
			return false;

	return true;
}

const String readTXXXString(const ID3v2::Tag *tag, const String &name)
{
	const ID3v2::FrameList &fl = tag->frameListMap()["TXXX"];
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it)
	{
		const ID3v2::TextIdentificationFrame *tif = dynamic_cast<const ID3v2::TextIdentificationFrame *>(*it);
		const StringList sl = tif->fieldList();
		if (sl.size() >= 2)
			if (is_iequal(sl[0].toWString(), name.toWString()))
				return sl[1];
	}

	return emptyString;
}


unsigned char normaliseRating(int rat)
{
	if (rat > 0)
		switch (rat)
		{
			case 1: return RATING_ONE_STAR_SET;
			case 2: return RATING_TWO_STARS_SET;
			case 3: return RATING_THREE_STARS_SET;
			case 4: return RATING_FOUR_STARS_SET;
			case 5: return RATING_FIVE_STARS_SET;
			default:
				{
					if (rat < 100)
						return rat;
					if (rat < 255)
						return static_cast<unsigned char>(rat*100/255.f);
				}
		}
	return RATING_UNRATED_SET;
}

unsigned char readrating(const APE::Tag *tag)
{
	const StringList& lst = tag->itemListMap()[L"rating"].values();
	if (lst.size())
		return normaliseRating(lst[0].toInt());
	return RATING_UNRATED_SET;
}


unsigned char readrating(const ASF::Tag *tag)
{
	return normaliseRating(tag->rating().toInt());
}

unsigned char readrating(const ID3v2::Tag *tag)
{
	// First attempt to read from 4.17 Popularimeter. (http://www.id3.org/id3v2.4.0-frames)
	//    <Header for 'Popularimeter', ID: "POPM">
    // Email to user   <text string> $00
    // Rating          $xx
    // Counter         $xx xx xx xx (xx ...)
	// The rating is 1-255 where 1 is worst and 255 is best. 0 is unknown.

	const ID3v2::FrameList &fl = tag->frameListMap()["POPM"];
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it)
		if (ID3v2::UnknownFrame *fr = dynamic_cast<ID3v2::UnknownFrame *>(*it))
		{
			const ByteVector &s = fr->data();

			// Locate the null byte.
			ByteVector::ConstIterator sp = std::find(s.begin(), s.end(), 0);

			// If we found it, and the string continues, at least another character, 
			//  the continued character is the rating byte.
			if (sp != s.end() && ++sp != s.end())
				return static_cast<unsigned char>(
					static_cast<unsigned char>(*sp)*100/255.f);
		}
	return normaliseRating(readTXXXString(tag, "rating").toInt());
}

unsigned char readrating(const Ogg::XiphComment *tag)
{
	const StringList &sl = tag->fieldListMap()["RATING"];
	for (StringList::ConstIterator it = sl.begin(); it != sl.end(); ++it)
		if (int i = it->toInt())
			return normaliseRating(i);
	return RATING_UNRATED_SET;
}

std::wstring readalbumArtist(const APE::Tag *tag)
{
	const StringList& lst = tag->itemListMap()[L"Album Artist"].values();
	if (lst.size())
		return lst[0].toWString();
	throw std::domain_error("no album artist");
}

std::wstring readalbumArtist(const ASF::Tag *tag)
{
	const char *name = "WM/AlbumArtist";

	// Ew ew ew.
	const ASF::AttributeListMap &alm = const_cast<ASF::Tag *>(tag)->attributeListMap();
	if (alm.contains(name))
		return alm[name][0].toString().toWString();

	throw std::domain_error("no asf");
}

std::wstring readalbumArtist(const ID3v2::Tag *tag)
{
	const ID3v2::FrameList &fl = tag->frameListMap()["TPE2"];
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it)
		if (ID3v2::TextIdentificationFrame *fr = dynamic_cast<ID3v2::TextIdentificationFrame *>(*it))
			return fr->toString().toWString();
	throw std::domain_error("no id3v2");
}

std::wstring readalbumArtist(const Ogg::XiphComment *tag)
{
	const StringList &sl = tag->fieldListMap()["ALBUMARTIST"];
	for (StringList::ConstIterator it = sl.begin(); it != sl.end(); ++it)
		return it->toWString();
	throw std::domain_error("no xiph");
}


READER_FUNC(unsigned char, rating, return RATING_UNRATED_SET)
READER_FUNC(std::wstring, albumArtist, throw std::domain_error("not good"))
