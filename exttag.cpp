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

#define RANGE(x) (x).begin(), (x).end()

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

typedef std::vector<TagLib::String> tlstrvec_t;

const tlstrvec_t readTXXXStrings(const ID3v2::Tag *tag, const String &name)
{
	const ID3v2::FrameList &fl = tag->frameListMap()["TXXX"];
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it)
	{
		const ID3v2::TextIdentificationFrame *tif = dynamic_cast<const ID3v2::TextIdentificationFrame *>(*it);
		const StringList sl = tif->fieldList();
		if (sl.size() >= 2)
			if (is_iequal(sl[0].toWString(), name.toWString()))
				return tlstrvec_t(++sl.begin(), sl.end());
	}
	return tlstrvec_t();
}

const String readTXXXString(const ID3v2::Tag *tag, const String &name)
{
	const tlstrvec_t sl = readTXXXStrings(tag, name);
	if (!sl.size())
		return emptyString;
	return sl[0];
}

std::wstring readString(const APE::Tag *tag, const std::wstring &name)
{
	const StringList& lst = tag->itemListMap()[name].values();
	if (lst.size())
		return lst[0].toWString();
	throw std::domain_error("no ape");
}

std::wstring readString(const ASF::Tag *tag, const std::string &name)
{
	// Ew ew ew.
	const ASF::AttributeListMap &alm = const_cast<ASF::Tag *>(tag)->attributeListMap();
	if (alm.contains(name))
		return alm[name][0].toString().toWString();

	throw std::domain_error("no asf");

}

std::wstring readString(const Ogg::XiphComment *tag, const std::string &name)
{
	const StringList &sl = tag->fieldListMap()[name];
	for (StringList::ConstIterator it = sl.begin(); it != sl.end(); ++it)
		return it->toWString();
	throw std::domain_error("no xiph");
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

#define FOR_EACH_ID3_FRAME_UNKNOWN(name) {                                      \
	const ID3v2::FrameList &fl = tag->frameListMap()[name];                     \
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it) \
		if (ID3v2::UnknownFrame *fr = dynamic_cast<ID3v2::UnknownFrame *>(*it)) \
		{                                                                       \
			const ByteVector &s = fr->data();

#define FOR_EACH_ID3_FRAME_TIF(name) {                                          \
	const ID3v2::FrameList &fl = tag->frameListMap()[name];                     \
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it) \
		if (ID3v2::TextIdentificationFrame *fr = dynamic_cast<ID3v2::TextIdentificationFrame *>(*it))
			


unsigned char readrating(const ID3v2::Tag *tag)
{
	// First attempt to read from 4.17 Popularimeter. (http://www.id3.org/id3v2.4.0-frames)
	//    <Header for 'Popularimeter', ID: "POPM">
    // Email to user   <text string> $00
    // Rating          $xx
    // Counter         $xx xx xx xx (xx ...)
	// The rating is 1-255 where 1 is worst and 255 is best. 0 is unknown.

	FOR_EACH_ID3_FRAME_UNKNOWN("POPM")
			// Locate the null byte.
			ByteVector::ConstIterator sp = std::find(s.begin(), s.end(), 0);

			// If we found it, and the string continues, at least another character, 
			//  the continued character is the rating byte.
			if (sp != s.end() && ++sp != s.end())
				return static_cast<unsigned char>(
					static_cast<unsigned char>(*sp)*100/255.f);
		}
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
	return readString(tag, L"Album Artist");
}

std::wstring readalbumArtist(const ASF::Tag *tag)
{
	return readString(tag, "WM/AlbumArtist");
}

std::wstring readalbumArtist(const ID3v2::Tag *tag)
{
	FOR_EACH_ID3_FRAME_TIF("TPE2")
		return fr->toString().toWString();
	}
	throw std::domain_error("no id3v2");
}

std::wstring readalbumArtist(const Ogg::XiphComment *tag)
{
	return readString(tag, "ALBUMARTIST");
}

wstrvec_t toVector(StringList::ConstIterator beg, StringList::ConstIterator end)
{
	StringList::ConstIterator it = beg;
	wstrvec_t ret;
	while (it != end)
		ret.push_back(it++->toWString());
	return ret;
}

wstrvec_t toVector(const tlstrvec_t &vec)
{
	wstrvec_t ret;
	ret.reserve(vec.size());
	for (std::vector<TagLib::String>::const_iterator it = vec.begin(); it != vec.end(); ++it)
		ret.push_back(it->toWString());
	return ret;
}

wstrvec_t toVector(const StringList &lst)
{
	return toVector(RANGE(lst));
}

wstrvec_t readkeywords(const APE::Tag *tag)
{
	return toVector(tag->itemListMap()[L"Keywords"].values());
}

wstrvec_t readkeywords(const ASF::Tag *tag)
{
	const char *name = "WM/Category";

	// Ew ew ew.
	const ASF::AttributeListMap &alm = const_cast<ASF::Tag *>(tag)->attributeListMap();
	if (alm.contains(name))
	{
		ASF::AttributeList lst = alm[name];
		wstrvec_t ret; ret.reserve(lst.size());
		for (ASF::AttributeList::ConstIterator it = lst.begin(); it != lst.end(); ++it)
			ret.push_back(it->toString().toWString());
		return ret;
	}

	throw std::domain_error("no asf");
}

wstrvec_t readkeywords(const ID3v2::Tag *tag)
{
	const ID3v2::FrameList &fl = tag->frameListMap()["TXXX"];
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it)
	{
		const ID3v2::TextIdentificationFrame *tif = dynamic_cast<const ID3v2::TextIdentificationFrame *>(*it);
		const StringList sl = tif->fieldList();
		if (sl.size() >= 2)
			if (is_iequal(sl[0].toWString(), L"keywords"))
			{
				StringList::ConstIterator oo = sl.begin();
				++oo;
				return toVector(oo, sl.end());
			}
	}
	return wstrvec_t();
}

wstrvec_t readkeywords(const Ogg::XiphComment *tag)
{
	return toVector(tag->fieldListMap()["KEYWORDS"]);
}

SYSTEMTIME parseDate(const wstrvec_t &vec)
{
	if (!vec.size())
		return SYSTEMTIME();

	for (wstrvec_t::const_iterator it = vec.begin(); it != vec.end(); ++it)
		// more than just the year, fill it in full.
		if (it->size() >= string("xxxx-xx-xx").size())
		{
			try
			{
				SYSTEMTIME full = {};
				full.wYear = boost::lexical_cast<int>(it->substr(0,4));
				full.wMonth = boost::lexical_cast<int>(it->substr(5,2));
				full.wDay = boost::lexical_cast<int>(it->substr(8,2));
				return full;
			}
			catch (boost::bad_lexical_cast &)
			{
				OutputDebugString((L"TagLibHandler: '" + *it + L"' failed to full parse as a date").c_str());
			}
		}

	// Abandon, and just go with the year.
	SYSTEMTIME ret = {};
	ret.wYear = boost::lexical_cast<int>(vec.at(0));
	return ret;
}

SYSTEMTIME readreleasedate(const APE::Tag *tag)
{
	return parseDate(toVector(tag->itemListMap()[L"Year"].values()));
}

SYSTEMTIME readreleasedate(const ASF::Tag *tag)
{
	const char *name = "WM/Year";

	// Ew ew ew.
	const ASF::AttributeListMap &alm = const_cast<ASF::Tag *>(tag)->attributeListMap();
	if (alm.contains(name))
	{
		ASF::AttributeList lst = alm[name];
		wstrvec_t ret; ret.reserve(lst.size());
		for (ASF::AttributeList::ConstIterator it = lst.begin(); it != lst.end(); ++it)
			ret.push_back(it->toString().toWString());
		return parseDate(ret);
	}

	return SYSTEMTIME();
}

// I wrote the code below to attempt to recover from v2.3 tags (D:), and taglib just destroys all the data. \o/
// Never mind, leave it in, why not? The code is brittle anyway.
SYSTEMTIME readreleasedate(const ID3v2::Tag *tag)
{
	// Attempt to read the (sane) id3v2.4 tag:
	wstrvec_t drcs;
	FOR_EACH_ID3_FRAME_TIF("TDRC")
		drcs.push_back(fr->toString().toWString());
	}

	SYSTEMTIME point4 = parseDate(drcs);
	if (point4.wYear)
		return point4;

	// Abandon all hope and try to deal with 2.3's failure:
	tlstrvec_t years, days;
	FOR_EACH_ID3_FRAME_TIF("TYER")
		years.push_back(fr->toString());
	}

	// No data, abort:
	if (!years.size())
		return SYSTEMTIME();

	// DDMM, guaranteed 4 chars long.
	FOR_EACH_ID3_FRAME_TIF("TDAT")
		days.push_back(fr->toString());
	}

	// If there are mismathcing year and day numbers, we can't do anything clever, return the first year.
	if (years.size() != days.size())
	{
		SYSTEMTIME ret = {};
		ret.wYear = years[0].toInt();
		return ret;
	}

	wstrvec_t composed;
	composed.reserve(years.size());

	tlstrvec_t::const_iterator dit = days.begin();
	for (tlstrvec_t::const_iterator yit = years.begin(); yit != years.end(); ++yit, ++dit)
	{
		const std::wstring day = dit->toWString();
		// This could throw if the TDAT tag is misformed.
		composed.push_back(yit->toWString() + L"-" + day.substr(2,2) + L"-" + day.substr(0,2));
	}
	return parseDate(composed);
}

SYSTEMTIME readreleasedate(const Ogg::XiphComment *tag)
{
	return parseDate(toVector(tag->fieldListMap()["DATE"]));
}

std::wstring readcomposer(const APE::Tag *tag)
{
	return readString(tag, L"Composer");
}

std::wstring readcomposer(const ASF::Tag *tag)
{
	return readString(tag, "WM/Composer");
}

std::wstring readcomposer(const ID3v2::Tag *tag)
{
	FOR_EACH_ID3_FRAME_TIF("TCOM")
		return fr->toString().toWString();
	}
	throw std::domain_error("no id3v2");
}

std::wstring readcomposer(const Ogg::XiphComment *tag)
{
	const StringList &sl = tag->fieldListMap()["COMPOSER"];
	for (StringList::ConstIterator it = sl.begin(); it != sl.end(); ++it)
		return it->toWString();
	throw std::domain_error("no xiph");
}

READER_FUNC(unsigned char, rating, return RATING_UNRATED_SET)
READER_FUNC(std::wstring, albumArtist, throw std::domain_error("not good"))
READER_FUNC(wstrvec_t, keywords, return wstrvec_t())
READER_FUNC(SYSTEMTIME, releasedate, return SYSTEMTIME())
READER_FUNC(std::wstring, composer, throw std::domain_error("not good"))
