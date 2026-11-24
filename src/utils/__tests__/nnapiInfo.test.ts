import {Platform} from 'react-native';
import NativeHardwareInfo from '../../specs/NativeHardwareInfo';
import {getNNAPIInfo} from '../deviceCapabilities';

jest.mock('../../specs/NativeHardwareInfo', () => ({
  __esModule: true,
  default: {
    getNNAPIInfo: jest.fn(),
    getCPUInfo: jest.fn(),
    getGPUInfo: jest.fn(),
  },
}));

const getNNAPIInfoNative = NativeHardwareInfo.getNNAPIInfo as jest.Mock;

describe('getNNAPIInfo', () => {
  const originalOS = Platform.OS;

  beforeEach(() => {
    jest.clearAllMocks();
  });

  afterEach(() => {
    Platform.OS = originalOS;
  });

  it('returns the Android NNAPI inventory without claiming execution support', async () => {
    Platform.OS = 'android';
    const inventory = {
      available: true,
      devices: [
        {
          name: 'Qualcomm Snapdragon',
          type: 'ACCELERATOR',
          version: '1.0',
          featureLevel: 30,
        },
      ],
    };
    getNNAPIInfoNative.mockResolvedValue(inventory);

    await expect(getNNAPIInfo()).resolves.toEqual(inventory);
    expect(getNNAPIInfoNative).toHaveBeenCalledTimes(1);
  });

  it('returns an explicit unavailable result on iOS without calling Android native code', async () => {
    Platform.OS = 'ios';

    await expect(getNNAPIInfo()).resolves.toMatchObject({
      available: false,
      devices: [],
    });
    expect(getNNAPIInfoNative).not.toHaveBeenCalled();
  });

  it('contains native discovery errors instead of crashing the Device Info screen', async () => {
    Platform.OS = 'android';
    getNNAPIInfoNative.mockRejectedValue(
      new Error('NNAPI symbols unavailable'),
    );
    const warning = jest.spyOn(console, 'warn').mockImplementation(() => {});

    await expect(getNNAPIInfo()).resolves.toMatchObject({
      available: false,
      error: 'NNAPI symbols unavailable',
      devices: [],
    });

    warning.mockRestore();
  });
});
