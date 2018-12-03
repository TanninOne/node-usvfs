if (process.env.BOOST_PATH === undefined) {
    console.error('BOOST_PATH has to be set');
    process.exit(2);
    return;
}

const fs = require('fs');
const path = require('path');
const cp = require('child_process');

if (process.env.SDKVersion === undefined) {
    try {
        const sdkManifestPath = path.join(process.env['ProgramFiles(x86)'], 'Windows Kits', '10', 'SDKManifest.xml');
	console.log('sdk manifest file at', sdkManifestPath);
        const sdkManifest = fs.readFileSync(sdkManifestPath, { encoding: 'utf-8' });
        const sdkver = sdkManifest
            .split('\r\n')
            .map(line => line.trim())
            .find(line => line.startsWith('PlatformIdentity'))
            .replace(/.*Version=([0-9.]*).*/, '$1');
        process.env.SDKVersion = sdkver;
    } catch (err) {
        console.error('Failed to determine SDK version', err.message);
    }
}

// find vs2017 installation
const vsWhere = path.join(process.env['ProgramFiles(x86)'], 'Microsoft Visual Studio', 'Installer', 'vswhere.exe');
const vsConfig = cp.execSync(`"${vsWhere}"`).toString();
const vsInstPath = vsConfig.split('\r\n').find(line => line.startsWith('installationPath:')).substr(18);
process.env['vsInstallDir'] = vsInstPath + '\\';

const msbuildLib = require('msbuild');

const msbuild = new msbuildLib(); 
msbuild.version = '15.0';
msbuild.sourcePath = './usvfs/vsbuild/usvfs_dll.vcxproj';
msbuild.configuration = 'Release';
msbuild.overrideParams.push('/p:VisualStudioVersion=15.0');
msbuild.overrideParams.push('/P:Platform=x64');
if (process.env.SDKVersion !== undefined) {
    msbuild.overrideParams.push(`/P:WindowsTargetPlatformVersion=${process.env.SDKVersion}`);
}
msbuild.overrideParams.push(`/P:BOOST_PATH=${process.env.BOOST_PATH}`);
msbuild.overrideParams.push('/P:STAGING_BASE=..\\..\\usvfs_build');

let first = true;
msbuild.on('error', () => {
    if (first) {
        first = false;
        msbuild.build();
    }
});

msbuild.build();
