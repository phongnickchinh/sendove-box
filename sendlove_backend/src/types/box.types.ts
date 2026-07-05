export interface Box {
  boxId: string;
  macAddress: string;
  deviceSecret: string;
  firmwareVersion: string;
  isOnline: boolean;
  lastSeen: number;
  
  pairingInfo: {
    senderCode: string;
    receiverCode: string;
    senderUid?: string | null;
    receiverUid?: string | null;
  };
  
  wifiConfig?: {
    ssid: string;
    password?: string;
    status: 'pending_apply' | 'applied' | 'failed';
  };
}

export interface DevicePollingCache {
  hasNewMessage: boolean;
  latestMessageId: string | null;
  hasNewAlarms: boolean;
  hasNewWifiConfig: boolean;
  pollIntervalSeconds: number;
}
