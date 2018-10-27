if (process.env.BOOST_PATH === undefined) {
    console.error('BOOST_PATH has to be set');
    process.exit(2);
    return;
}

const path = require('path');
const cp = require('child_process');

// find vs2017 installation
const vsWhere = path.join(process.env['ProgramFiles(x86)'], 'Microsoft Visual Studio', 'Installer', 'vswhere.exe');
const vsConfig = cp.execSync(`"${vsWhere}"`).toString();
const vsInstPath = vsConfig.split('\r\n').find(line => line.startsWith('installationPath:')).substr(18);
console.log(vsInstPath);
process.env['vsInstallDir'] = vsInstPath + '\\';

const msbuildLib = require('msbuild');

const msbuild = new msbuildLib(); 
msbuild.version = '15.0';
msbuild.sourcePath = './usvfs/vsbuild/usvfs_dll.vcxproj';
msbuild.configuration = 'Release';
msbuild.overrideParams.push('/p:VisualStudioVersion=15.0');
msbuild.overrideParams.push('/P:Platform=x64');
msbuild.overrideParams.push(`/P:BOOST_PATH=${process.env.BOOST_PATH}`);
msbuild.overrideParams.push('/P:STAGING_BASE=..\\..\\usvfs_build');

msbuild.build();
