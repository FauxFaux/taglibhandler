#include "../exttag.h"
#include <windows.h>

#if 0
int main()
{
	const char *path = "../propdump/t.mp3";
	TagLib::FileRef r(path);
	int q,j;
	while (!(q = rating(r))) r = TagLib::FileRef(path);
	j = rating(r);
	DebugBreak();
	rating(r);
	
}
#endif

#if 1
int main()
{
	const char *path = "../propdump/tw.wma";
	TagLib::FileRef r(path);
	std::wstring q = albumArtist(r);
}
#endif