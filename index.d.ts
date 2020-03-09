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

export function CreateVFS(params: IUSVFSParameters): void;
export function ConnectVFS(params: IUSVFSParameters): void;
export function DisconnectVFS(): void;
export function InitLogging(toLocal?: boolean): void;
export function ClearVirtualMappings(): void;
export function VirtualLinkFile(source: string, destination: string, parameters: ILinkParameters): void;
export function VirtualLinkDirectoryStatic(source: string, destination: string, parameters: ILinkParameters): void;

export function CreateProcessHooked(applicationName: string, commandLine: string, currentDirectory: string, environment: any): void;
export function GetLogMessage(blocking?: boolean): string;
/**
 * poll log messages asynchronously (in a separate thread). Every log line will trigger a call to the callback and polling
 * will continue if the callback returns true
 * @param callback 
 */
export function PollLogMessages(logCB: (message: string) => boolean, callback: (err: Error) => void): void;
