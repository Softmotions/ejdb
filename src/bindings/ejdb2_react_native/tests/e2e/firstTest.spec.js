describe('EJDB2', () => {

  beforeEach(async () => {
    await device.reloadReactNative();
  });

  it('all', async () => {
      await new Promise((resolve) => setTimeout(resolve, 2000));
      await element(by.id('run')).tap();
      await waitFor(element(by.id('status'))).toExist().withTimeout(2000);
      await waitFor(element(by.id('status'))).toHaveText('OK').withTimeout(20000);
  });
});
