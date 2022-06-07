param (
	[string]$build_dir = "build",
	[string]$release_dir = "release",
	[string]$mode = "Release",
	[string]$arch = "x64",
	[switch]$skip_cmake = $false
)

mkdir $build_dir/Win32 -ea 0
mkdir $build_dir/x64 -ea 0
mkdir $release_dir -ea 0

if(($arch -ne "Win32") -and ($arch -ne "x64")) {
	echo "arch should be either Win32 or x64"
	exit 1
}

if(!($skip_cmake)) {
	if($arch -eq "x64") {
		$win32_injectee_only="-DPROXINJECTEE_ONLY=ON"
	} else {
		$win32_injectee_only=""
	}

	if($arch -eq "x64") {
		cmake -DCMAKE_BUILD_TYPE="$mode" -A x64 -S . -B $build_dir/x64	
	}
	cmake -DCMAKE_BUILD_TYPE="$mode" $win32_injectee_only -A Win32 -S . -B $build_dir/Win32
}

if($arch -eq "x64") {
	cmake --build $build_dir/x64 --config $mode -j $env:NUMBER_OF_PROCESSORS
}
cmake --build $build_dir/Win32 --config $mode -j $env:NUMBER_OF_PROCESSORS

cp $build_dir/$arch/$mode/* $release_dir -Force
cp $build_dir/$arch/*.dll $release_dir -Force
cp $build_dir/$arch/resources $release_dir -Recurse -Force

if($arch -eq "x64") {
	cp $build_dir/Win32/$mode/proxinjectee.dll $release_dir/proxinjectee32.dll -Force
	cp $build_dir/Win32/$mode/wow64-address-dumper.exe $release_dir -Force
}
