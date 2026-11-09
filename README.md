# Quby - MegascanImporter

Open `Quby - MegascanImporter.exe` in the Windows bundle, select **Open Fab**, and browse Quixel inside the app. If Fab offers a download, choose the format and quality on Fab. Quby asks before saving supported asset files. The Downloads page shows this session's transfers and opens their folders.

The build uses the installed Microsoft Edge WebView2 Runtime and keeps its own Fab profile. The user verified a real Fab download.

## Export to Blender

In **Downloads**, select **Export to Blender**. For the first export, save and close Blender so Quby can register its managed **Megascans** library. Open Blender afterward and select that library in the Asset Browser. Refresh the Asset Browser after subsequent exports or removals. You can also export a local file from Quby's **Blender Library** page.

The exporter handles FBX, glTF/GLB and ZIP packages containing a supported mesh or recognized surface texture maps. It also accepts a single texture as a material asset. Quby keeps its managed library separate from other Blender libraries. **Remove** moves the exported file to the Recycle Bin and retains the source download. Geometry Nodes setup is pending.

## Blender preview

`blender/preview.py` applies the cached Fab preview automatically. When you confirm a download from a Fab listing, Quby saves its public preview as a JPEG no larger than 256 pixels on either edge. Older downloads and listings without a usable image use a packaged preview or a Blender render. Export reopens and checks each saved asset and thumbnail before publishing the file to the library.

## Build

Use an x64 Visual Studio C++ toolchain, Qt 6.12 MSVC x64, CMake, and Microsoft's WebView2 SDK. Download the official `Microsoft.Web.WebView2` NuGet package (tested with `1.0.4314.35`) and extract it into a folder. Pass that folder as `WEBVIEW2_SDK`.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64 -DWEBVIEW2_SDK=C:/path/to/unpacked-sdk
cmake --build build --config Release
$env:Path = 'C:\Qt\6.12.0\msvc2022_64\bin;' + $env:Path
ctest --test-dir build -C Release --output-on-failure
& 'C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe' --release --qmldir qml "build/Release/Quby - MegascanImporter.exe"
```

Run the Blender preview test with `blender --background --factory-startup --python-exit-code 1 --python tests/preview_smoke.py -- C:/path/to/test.blend`.

## License

The project source is available under the MIT license. Qt, Blender, the WebView2 SDK and runtime, and assets acquired from Fab have their own licenses. The repository contains no Fab asset downloads. Quby is an independent project and is not affiliated with Epic Games or the Blender Foundation.
