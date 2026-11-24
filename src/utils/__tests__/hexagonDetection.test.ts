import {getAvailableDevices} from '../deviceSelection';
import {getHexagonInfo, isHexagonVersionSupported} from '../hexagonDetection';

jest.mock('../deviceSelection', () => ({
  getAvailableDevices: jest.fn(),
}));

const getAvailableDevicesMock = getAvailableDevices as jest.Mock;

describe('Hexagon generation detection', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    getAvailableDevicesMock.mockResolvedValue([]);
  });

  it('recognizes Snapdragon 662 as V66 hardware without claiming a GGUF backend', async () => {
    await expect(getHexagonInfo('Qualcomm SM6115')).resolves.toEqual([
      {
        version: '66',
        deviceName: 'NNAPI candidate (execution unverified)',
        soc: 'Snapdragon 662 / SM6115',
        supported: false,
      },
    ]);
    expect(isHexagonVersionSupported('66')).toBe(false);
  });

  it('does not mistake a V66 DSP for an HTP device', async () => {
    getAvailableDevicesMock.mockResolvedValue([
      {
        backend: 'HTP',
        type: 'accel',
        deviceName: 'HTP0',
        maxMemorySize: 0,
      },
    ]);

    const info = await getHexagonInfo('SM6115');

    expect(info).toHaveLength(1);
    expect(info[0].version).toBe('66');
    expect(info[0].supported).toBe(false);
  });

  it('keeps modern HTP discovery dependent on a real HTP backend device', async () => {
    await expect(getHexagonInfo('SM8550')).resolves.toEqual([]);
    getAvailableDevicesMock.mockResolvedValue([
      {
        backend: 'HTP',
        type: 'accel',
        deviceName: 'HTP0',
        maxMemorySize: 0,
      },
    ]);

    await expect(getHexagonInfo('SM8550')).resolves.toEqual([
      {
        version: '750',
        deviceName: 'HTP0',
        soc: 'Snapdragon 8 Gen 2',
        supported: true,
      },
    ]);
  });
});
