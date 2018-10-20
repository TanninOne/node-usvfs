export interface IUSVFSParameters {
  instanceName: string;
  debugMode: boolean;
  logLevel: 0 | 1 | 2 | 3;
  crashDumpType: 0 | 1 | 2 | 3;
  crashDumpPath: string;
}

export interface ILinkParameters {
  failIfExists?: boolean;
  monitorChanges?: boolean;
  createTarget?: boolean;
  recursive?: boolean;
}

export function CreateVFS(params: IUSVFSParameters);
export function ConnectVFS(params: IUSVFSParameters);
export function DisconnectVFS();

export function ClearVirtualMappings();
export function VirtualLinkFile(source: string, destination: string, parameters: ILinkParameters);
export function VirtualLinkDirectoryStatic(source: string, destination: string, parameters: ILinkParameters);

export function CreateProcessHooked(applicationName: string, commandLine: string, currentDirectory: string, environment: any);
