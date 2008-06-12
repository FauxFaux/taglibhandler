Taglib Property Handler: http://sourceforge.net/projects/taglibhandler/

-- Alpha release.

This is an alpha release. It's hard to install, and, as all of the components are built as debug versions, 
  it should be very slow. Don't use it if you don't know what you're doing.

-- Install instructions (Vista only):

1) Run tlhsetup2.exe. It needs Administrator access, and will prompt for it if you have UAC enabled.

   - It is 64-bit Windows aware.

-- Manual install instructions:

1) Register the 32-bit TagLibHandler.dll (from this directory), ie.

    > regsvr32 TagLibHandler.dll

     This will give an illogical error message if not run with sufficient permissions. Possibly:
      >  The module "TagLibHandler.dll" was loaded but the call to DllRegisterServer failed with error code 0x80070005.

1a) If you are using a 64-bit OS, also register the x64 version of the component.

     It's worth having both installed as frequently you'll use x32 apps without realising.

2) Inform the property system that you'd like the handler to be used. This involves:

  a) Navigate to HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers
  b) Create (or find) a Key (folder) with the name of the file extension you wish to use.
  c) Edit it's (Default) such that the value is: {66AC53F1-EB5D-4af9-861E-E3AE9C07EF14}

  ie. create a Key called HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.ogg
    and set it's (Default) value to {66AC53F1-EB5D-4af9-861E-E3AE9C07EF14}.

3) It's safe to replace the system associations, such as .mp3, although, at this stage, you will lose functionality by doing so.

   You may want to back-up any data you overwrite.


-- Lost functionality by using Taglib Handler instead of the Windows Default.

   You will lose: The ability to write to tags, and the ability to view some tags.
   You will gain: The ability to read id3v2.4 tags with utf-8.


-- License (MIT):

Copyright (c) 2008 Chris West (Faux)

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
