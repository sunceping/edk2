import os
import stat
import shutil
import sys
import time

# if os.path.exists('Z:/'):
#     os.system('subst z: /d')
# os.system('subst z: C:\edk2-staging_Zhiqiang')

def readonly_handler(func, path, excinfo): 
    os.chmod(path, stat.S_IWRITE)
    func(path)  
# if os.path.exists('Z:\Build'):
#     shutil.rmtree('Z:\Build', onerror=readonly_handler)
os.environ["PYTHON_HOME"] = r"C:\Python38"
# os.chdir('Z:')

pwd = "D:\Ceping\edk2_ww4405"
os.chdir(pwd)

os.system(r'edksetup.bat && build -p EmulatorPkg\EmulatorPkg.dsc -t VS2019 -a X64 -b DEBUG -j build_DEBUG.log')
os.system(r'edksetup.bat && build -p DeviceSecurityTestPkg\DeviceSecurityTestPkg.dsc -t VS2019 -a X64 -b DEBUG -j build_DEBUG.log')
os.system(r'copy /Y Build\DeviceSecurityTestPkg\DEBUG_VS2019\X64\*.efi Build\EmulatorX64\DEBUG_VS2019\X64')


sys.exit()
WinHost_exe_path = pwd + r'\Build\EmulatorX64\DEBUG_VS2019\X64'
test_output = os.path.join(WinHost_exe_path,'test_output')
startup_nsh = os.path.join(WinHost_exe_path,'startup.nsh')
test_output_log = os.path.join(test_output, 'log')
test_output_bootlog = os.path.join(test_output, 'bootlog')

if os.path.exists(test_output):
    shutil.rmtree(test_output, onerror=readonly_handler)
os.mkdir(test_output)
os.chdir(test_output)
os.system('echo %date% > env.txt')
os.system('echo %time% >> env.txt')
os.system('git log --pretty="%H" -1 >> env.txt')

os.mkdir(test_output_log)
os.mkdir(test_output_bootlog)
for root,dirs,files in os.walk(os.path.join(sys.path[0], 'TestConfig')):
    for filespath in files:
        # if "TestConfig_03.nsh" not in filespath:
        #     continue
        print (os.path.join(root,filespath))
        LogFileName = os.path.split(filespath)[1].replace(r'.nsh' ,r'.log')
        print(LogFileName)
        shutil.copy(os.path.join(root,filespath), startup_nsh)
        os.chdir(WinHost_exe_path)
        if os.path.exists(os.path.join(test_output_log, LogFileName)):
            os.remove(os.path.join(test_output_log, LogFileName))
        # os.system(r'start WinHost.exe')
        bootLogFileName = os.path.split(filespath)[1].replace(r'.nsh' ,r'_boot.log')
        bootlogfile = os.path.join(test_output_bootlog, bootLogFileName)
        cmd = "WinHost.exe > " + bootlogfile + " 2>&1"
        os.system(cmd)

        count = 0
        while not os.path.exists(os.path.join(test_output_log, LogFileName)):
            #crash before creating log file, then create a fake log file and break
            if(count > 15):
                os.system('echo fake log file> %s'%(os.path.join(test_output_log, LogFileName)))
                print('waiting too long for test case finish, so exit')
                break
            time.sleep(3)
            print('waiting for test case finish')
            count+=1
        else:
            time.sleep(10)
            print('test case finish')
        time.sleep(3)
        # os.system(r'taskkill /IM WinHost.exe /F /T')
        # time.sleep(3)