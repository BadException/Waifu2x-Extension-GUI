﻿/*
    Copyright (C) 2021  Aaron Feng

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    My Github homepage: https://github.com/AaronFeng753
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

int MainWindow::Waifu2x_NCNN_Vulkan_Image(int rowNum,bool ReProcess_MissingAlphaChannel)
{
    //============================= 读取设置 ================================
    int DenoiseLevel = ui->spinBox_DenoiseLevel_image->value();
    bool DelOriginal = (ui->checkBox_DelOriginal->isChecked()||ui->checkBox_ReplaceOriginalFile->isChecked());
    QString OutPutPath_Final ="";
    //========================= 拆解map得到参数 =============================
    //将状态设定到处理中
    emit Send_Table_image_ChangeStatus_rowNumInt_statusQString(rowNum, "Processing");
    //得到原文件路径
    QString SourceFile_fullPath = Table_model_image->item(rowNum,2)->text();
    if(!QFile::exists(SourceFile_fullPath))//判断源文件是否存在
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [File does not exist.]"));
        emit Send_Table_image_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        mutex_ThreadNumRunning.lock();
        ThreadNumRunning--;
        mutex_ThreadNumRunning.unlock();//线程数量统计-1
        return 0;
    }
    //========= 转换到 PNG =========
    QString SourceFile_fullPath_Original = SourceFile_fullPath;
    SourceFile_fullPath = Imgae_PreProcess(SourceFile_fullPath_Original,ReProcess_MissingAlphaChannel);
    //===============
    int ScaleRatio=2;
    bool CustRes_isEnabled = false;
    int CustRes_height=0;
    int CustRes_width=0;
    //检查是是否有自定义分辨率
    if(CustRes_isContained(SourceFile_fullPath_Original))
    {
        CustRes_isEnabled=true;
        QMap<QString, QString> Res_map = CustRes_getResMap(SourceFile_fullPath_Original);//res_map["fullpath"],["height"],["width"]
        ScaleRatio = CustRes_CalNewScaleRatio(SourceFile_fullPath,Res_map["height"].toInt(),Res_map["width"].toInt());
        CustRes_height=Res_map["height"].toInt();
        CustRes_width=Res_map["width"].toInt();
    }
    else
    {
        double ScaleRatio_double_tmp = ui->doubleSpinBox_ScaleRatio_image->value();
        if(ScaleRatio_double_tmp == (int)ScaleRatio_double_tmp)
        {
            ScaleRatio = qRound(ScaleRatio_double_tmp);
        }
        else
        {
            CustRes_isEnabled=true;
            QMap<QString, QString> Res_map = DoubleScaleRatio_Cal_NewScaleRatio_NewHW(SourceFile_fullPath,ScaleRatio_double_tmp);
            //====
            if(Res_map.isEmpty())
            {
                emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to read the resolution of the source file.]"));
                emit Send_Table_image_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
                emit Send_progressbar_Add();
                mutex_ThreadNumRunning.lock();
                ThreadNumRunning--;
                mutex_ThreadNumRunning.unlock();//线程数量统计-1
                return 0;
            }
            //====
            ScaleRatio = Res_map["ScaleRatio"].toInt();
            CustRes_height = Res_map["Height_new"].toInt();
            CustRes_width = Res_map["width_new"].toInt();
        }
    }
    //===============
    QFileInfo fileinfo(SourceFile_fullPath);
    QString file_name = file_getBaseName(SourceFile_fullPath);
    QString file_ext = fileinfo.suffix();
    QString file_path = file_getFolderPath(fileinfo);
    QString OutPut_Path = file_path + "/" + file_name + "_waifu2x_"+QString::number(ScaleRatio, 10)+"x_"+QString::number(DenoiseLevel, 10)+"n_"+file_ext+".png";
    //============================== 放大 =======================================
    QProcess *Waifu2x = new QProcess();
    QString Waifu2x_folder_path = Waifu2x_ncnn_vulkan_FolderPath;
    QString program = Waifu2x_ncnn_vulkan_ProgramPath;
    //==========
    int ScaleRatio_tmp=Calculate_Temporary_ScaleRatio_W2xNCNNVulkan(ScaleRatio);
    //======
    QString InputPath_tmp = SourceFile_fullPath;
    QString OutputPath_tmp ="";
    int DenoiseLevel_tmp = DenoiseLevel;
    QString ErrorMSG="";
    QString StanderMSG="";
    QString cmd="";
    //======
    for(int retry=0; retry<(ui->spinBox_retry->value()+ForceRetryCount); retry++)
    {
        bool waifu2x_qprocess_failed = false;
        InputPath_tmp = SourceFile_fullPath;
        OutputPath_tmp ="";
        DenoiseLevel_tmp = DenoiseLevel;
        for(int i=2; i<=ScaleRatio_tmp; i*=2)
        {
            ErrorMSG="";
            StanderMSG="";
            //==========
            OutputPath_tmp = file_path + "/" + file_name + "_waifu2x_"+QString::number(i, 10)+"x_"+QString::number(DenoiseLevel, 10)+"n_"+file_ext+".png";
            cmd = "\"" + program + "\"" + " -i " + "\"" + InputPath_tmp + "\"" + " -o " + "\"" + OutputPath_tmp + "\"" + " -s " + "2" + " -n " + QString::number(DenoiseLevel_tmp, 10) + Waifu2x_NCNN_Vulkan_ReadSettings();
            Waifu2x->start(cmd);
            while(!Waifu2x->waitForStarted(100)&&!QProcess_stop) {}
            while(!Waifu2x->waitForFinished(500)&&!QProcess_stop)
            {
                if(waifu2x_STOP)
                {
                    Waifu2x->close();
                    if(i>2)
                    {
                        QFile::remove(InputPath_tmp);
                    }
                    QFile::remove(OutputPath_tmp);
                    emit Send_Table_image_ChangeStatus_rowNumInt_statusQString(rowNum, "Interrupted");
                    mutex_ThreadNumRunning.lock();
                    ThreadNumRunning--;
                    mutex_ThreadNumRunning.unlock();
                    return 0;
                }
                ErrorMSG.append(Waifu2x->readAllStandardError().toLower());
                StanderMSG.append(Waifu2x->readAllStandardOutput().toLower());
                if(ErrorMSG.contains("failed")||StanderMSG.contains("failed"))
                {
                    waifu2x_qprocess_failed = true;
                    Waifu2x->close();
                    if(i>2)
                    {
                        QFile::remove(InputPath_tmp);
                    }
                    QFile::remove(OutputPath_tmp);
                    break;
                }
            }
            //===============
            if(waifu2x_qprocess_failed)break;
            //===============
            ErrorMSG.append(Waifu2x->readAllStandardError().toLower());
            StanderMSG.append(Waifu2x->readAllStandardOutput().toLower());
            if(ErrorMSG.contains("failed")||StanderMSG.contains("failed"))
            {
                waifu2x_qprocess_failed = true;
                if(i>2)
                {
                    QFile::remove(InputPath_tmp);
                }
                QFile::remove(OutputPath_tmp);
                break;
            }
            //===============
            if(i>2)
            {
                QFile::remove(InputPath_tmp);
            }
            DenoiseLevel_tmp = -1;
            InputPath_tmp = OutputPath_tmp;
        }
        //========= 检测是否成功,是否需要重试 ============
        if(QFile::exists(OutputPath_tmp)&&!waifu2x_qprocess_failed)
        {
            break;
        }
        else
        {
            QFile::remove(OutputPath_tmp);
            if(retry==ui->spinBox_retry->value()+(ForceRetryCount-1))break;
            emit Send_TextBrowser_NewMessage(tr("Automatic retry, please wait."));
            Delay_sec_sleep(5);
        }
    }
    if(!QFile::exists(OutputPath_tmp))
    {
        if(SourceFile_fullPath_Original!=SourceFile_fullPath)
        {
            QFile::remove(SourceFile_fullPath);
        }
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath_Original+tr("]. Error: [Unable to scale the picture.]"));
        emit Send_Table_image_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        mutex_ThreadNumRunning.lock();
        ThreadNumRunning--;
        mutex_ThreadNumRunning.unlock();
        return 0;
    }
    OutPutPath_Final = OutputPath_tmp;
    //============================ 调整大小 ====================================================
    if(ScaleRatio_tmp != ScaleRatio||CustRes_isEnabled)
    {
        int New_height=0;
        int New_width=0;
        if(CustRes_isEnabled)
        {
            New_height= CustRes_height;
            New_width= CustRes_width;
        }
        else
        {
            QMap<QString,int> res_map_OriginalFile = Image_Gif_Read_Resolution(SourceFile_fullPath);
            New_height = res_map_OriginalFile["height"]*ScaleRatio;
            New_width = res_map_OriginalFile["width"]*ScaleRatio;
        }
        QImage qimage_adj(OutputPath_tmp);
        //读取放大后的图片并调整大小
        QImage qimage_adj_scaled = qimage_adj.scaled(New_width,New_height,CustRes_AspectRatioMode,Qt::SmoothTransformation);
        QImageWriter qimageW_adj;
        qimageW_adj.setFormat("png");
        qimageW_adj.setFileName(OutPut_Path);
        if(qimageW_adj.canWrite())
        {
            qimageW_adj.write(qimage_adj_scaled);
        }
        QFile::remove(OutputPath_tmp);
        if(QFile::exists(OutPut_Path)==false)
        {
            emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to resize the scaled picture to the target size]"));
            emit Send_Table_image_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
            emit Send_progressbar_Add();
            mutex_ThreadNumRunning.lock();
            ThreadNumRunning--;
            mutex_ThreadNumRunning.unlock();//线程数量统计-1
            return 0;
        }
        OutPutPath_Final = OutPut_Path;
    }
    if(CustRes_isEnabled)
    {
        QString OutPut_Path_CustRes = file_path + "/" + file_name + "_waifu2x_"+QString::number(CustRes_width, 10)+"x"+QString::number(CustRes_height, 10)+"_"+QString::number(DenoiseLevel, 10)+"n_"+file_ext+".png";
        QFile::remove(OutPut_Path_CustRes);
        QFile::rename(OutPut_Path,OutPut_Path_CustRes);
        OutPutPath_Final = OutPut_Path_CustRes;
    }
    //=========================== 另存为JPG&压缩JPG ===========================================
    OutPutPath_Final = SaveImageAs_FormatAndQuality(SourceFile_fullPath_Original,OutPutPath_Final,ScaleRatio,true,DenoiseLevel);
    //================== 检查是否丢失了透明通道 =====================
    if(ReProcess_MissingAlphaChannel==false && ui->comboBox_version_Waifu2xNCNNVulkan->currentIndex()==0)
    {
        QImage QImage_source(SourceFile_fullPath_Original);
        QImage QImage_Final(OutPutPath_Final);
        if(QImage_source.hasAlphaChannel()==true && QImage_Final.hasAlphaChannel()==false && ui->checkBox_AutoDetectAlphaChannel->isChecked())
        {
            QFile::remove(OutPutPath_Final);
            emit Send_TextBrowser_NewMessage(tr("Since the Alpha channel in the resulting image is lost, this image will be reprocessed to fix it:[")+SourceFile_fullPath_Original+tr("].\nIf the reprocessing happens a lot, you should consider enable [Always pre-process images with Alpha Channel] in [Additional settings]."));
            return Waifu2x_NCNN_Vulkan_Image(rowNum,true);
        }
    }
    //============================= 删除原文件 & 更新table status ============================
    if(SourceFile_fullPath_Original!=SourceFile_fullPath)
    {
        QFile::remove(SourceFile_fullPath);
        SourceFile_fullPath = SourceFile_fullPath_Original;
    }
    if(DelOriginal)
    {
        if(ReplaceOriginalFile(SourceFile_fullPath,OutPutPath_Final)==false)
        {
            if(QAction_checkBox_MoveToRecycleBin_checkBox_DelOriginal->isChecked())
            {
                file_MoveToTrash(SourceFile_fullPath);
            }
            else
            {
                QFile::remove(SourceFile_fullPath);
            }
        }
        emit Send_Table_image_ChangeStatus_rowNumInt_statusQString(rowNum, "Finished, original file deleted");
    }
    else
    {
        emit Send_Table_image_ChangeStatus_rowNumInt_statusQString(rowNum, "Finished");
    }
    //========== 移动到输出路径 =========
    if(ui->checkBox_OutPath_isEnabled->isChecked())
    {
        MoveFileToOutputPath(OutPutPath_Final,SourceFile_fullPath);
    }
    //============================ 更新进度条 =================================
    emit Send_progressbar_Add();
    //=========================== 更新线程数量统计==============================
    mutex_ThreadNumRunning.lock();
    ThreadNumRunning--;
    mutex_ThreadNumRunning.unlock();//线程数量统计-1
    return 0;
}

/*

  =============================================================================================
  =============================================================================================
  ============================= Waifu2x_NCNN_Vulkan_GIF =======================================
  =============================================================================================
  =============================================================================================

*/

int MainWindow::Waifu2x_NCNN_Vulkan_GIF(int rowNum)
{
    //============================= 读取设置 ================================
    int ScaleRatio = ui->doubleSpinBox_ScaleRatio_gif->value();
    int DenoiseLevel = ui->spinBox_DenoiseLevel_gif->value();
    bool DelOriginal = (ui->checkBox_DelOriginal->isChecked()||ui->checkBox_ReplaceOriginalFile->isChecked());
    bool OptGIF = ui->checkBox_OptGIF->isChecked();
    QString OutPutPath_Final ="";
    //========================= 拆解map得到参数 =============================
    emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Processing");
    QString SourceFile_fullPath = Table_model_gif->item(rowNum,2)->text();
    if(!QFile::exists(SourceFile_fullPath))
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [File does not exist.]"));
        emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;
    }
    //==========================
    bool CustRes_isEnabled = false;
    int CustRes_height=0;
    int CustRes_width=0;
    if(CustRes_isContained(SourceFile_fullPath))
    {
        CustRes_isEnabled=true;
        QMap<QString, QString> Res_map = CustRes_getResMap(SourceFile_fullPath);//res_map["fullpath"],["height"],["width"]
        CustRes_height=Res_map["height"].toInt();
        CustRes_width=Res_map["width"].toInt();
    }
    //==========================
    QFileInfo fileinfo(SourceFile_fullPath);
    QString file_name = file_getBaseName(SourceFile_fullPath);
    QString file_ext = fileinfo.suffix();
    QString file_path = file_getFolderPath(fileinfo);
    QString ResGIFPath = file_path + "/" + file_name + "_waifu2x_"+QString::number(ScaleRatio, 10)+"x_"+QString::number(DenoiseLevel, 10)+"n.gif";
    //=========================== 获取帧时间 ====================================
    int GIF_Duration = Gif_getDuration(SourceFile_fullPath);
    if(GIF_Duration==0)//检查是否成功获取duration
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Can't get Duration value of GIF file.]"));
        emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    //============================== 拆分 ==========================================
    QString SplitFramesFolderPath = file_path+"/"+file_name+"_SplitFrames_W2xEX";//拆分后存储frame的文件夹
    Gif_splitGif(SourceFile_fullPath,SplitFramesFolderPath);
    //============================== 扫描获取文件名 ===============================
    QStringList Frame_fileName_list = file_getFileNames_in_Folder_nofilter(SplitFramesFolderPath);
    if(Frame_fileName_list.isEmpty())//检查是否成功拆分gif
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Can't split GIF into frames.]"));
        emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        file_DelDir(SplitFramesFolderPath);
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    //===========建立存储放大后frame的文件夹===========
    QString ScaledFramesFolderPath = SplitFramesFolderPath+"/ScaledFrames_W2xEX";
    if(file_isDirExist(ScaledFramesFolderPath))
    {
        file_DelDir(ScaledFramesFolderPath);
        file_mkDir(ScaledFramesFolderPath);
    }
    else
    {
        file_mkDir(ScaledFramesFolderPath);
    }
    //==========开始放大==========================
    //读取放大倍数
    int ScaleRatio_Original = 2;
    if(CustRes_isEnabled == true)
    {
        ScaleRatio_Original = CustRes_CalNewScaleRatio(SourceFile_fullPath,CustRes_height,CustRes_width);
    }
    else
    {
        ScaleRatio_Original = ui->doubleSpinBox_ScaleRatio_gif->value();
    }
    int ScaleRatio_Max=Calculate_Temporary_ScaleRatio_W2xNCNNVulkan(ScaleRatio_Original);
    bool isOverScaled = (ScaleRatio_Max!=ScaleRatio_Original);
    //初始化进度讯息
    int NumOfSplitFrames = Frame_fileName_list.size();
    if(ui->checkBox_ShowInterPro->isChecked())
    {
        int NumOfRounds=0;
        for(int ScaleRatio_Current_tmp=2; ScaleRatio_Current_tmp<=ScaleRatio_Max; ScaleRatio_Current_tmp*=2)
        {
            NumOfRounds++;
        }
        emit Send_CurrentFileProgress_Start(file_name+"."+file_ext,NumOfSplitFrames*NumOfRounds);
    }
    //读取配置讯息
    QString Waifu2x_NCNN_Vulkan_Settings_str = Waifu2x_NCNN_Vulkan_ReadSettings_Video_GIF(ui->spinBox_ThreadNum_gif_internal->value());
    QProcess *Waifu2x = new QProcess();
    QString program = Waifu2x_ncnn_vulkan_ProgramPath;
    bool waifu2x_qprocess_failed = false;
    int DenoiseLevel_tmp = DenoiseLevel;
    DenoiseLevel_tmp = DenoiseLevel;
    int CountFinishedRounds=0;
    //====
    for(int ScaleRatio_Current_tmp=2; ScaleRatio_Current_tmp<=ScaleRatio_Max && Frame_fileName_list.isEmpty()==false; ScaleRatio_Current_tmp*=2)
    {
        for(int retry=0; retry<(ui->spinBox_retry->value()+ForceRetryCount); retry++)
        {
            QString ErrorMSG="";
            QString StanderMSG="";
            //==========
            waifu2x_qprocess_failed = false;
            QString cmd = "\"" + program + "\"" + " -i " + "\"" + SplitFramesFolderPath + "\"" + " -o " + "\"" + ScaledFramesFolderPath + "\"" + " -s " + "2" + " -n " + QString::number(DenoiseLevel_tmp, 10) + Waifu2x_NCNN_Vulkan_Settings_str;
            Waifu2x->start(cmd);
            while(!Waifu2x->waitForStarted(100)&&!QProcess_stop) {}
            while(!Waifu2x->waitForFinished(650)&&!QProcess_stop)
            {
                if(waifu2x_STOP)
                {
                    Waifu2x->close();
                    file_DelDir(SplitFramesFolderPath);
                    emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Interrupted");
                    mutex_ThreadNumRunning.lock();
                    ThreadNumRunning--;
                    mutex_ThreadNumRunning.unlock();
                    return 0;
                }
                ErrorMSG.append(Waifu2x->readAllStandardError().toLower());
                StanderMSG.append(Waifu2x->readAllStandardOutput().toLower());
                if(ErrorMSG.contains("failed")||StanderMSG.contains("failed"))
                {
                    waifu2x_qprocess_failed = true;
                    Waifu2x->close();
                    break;
                }
                if(ui->checkBox_ShowInterPro->isChecked())
                {
                    emit Send_CurrentFileProgress_progressbar_SetFinishedValue((NumOfSplitFrames*CountFinishedRounds)+file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath).size());
                }
            }
            //===============
            if(waifu2x_qprocess_failed==false)
            {
                ErrorMSG.append(Waifu2x->readAllStandardError().toLower());
                StanderMSG.append(Waifu2x->readAllStandardOutput().toLower());
                if(ErrorMSG.contains("failed")||StanderMSG.contains("failed"))
                {
                    waifu2x_qprocess_failed = true;
                }
            }
            //========= 检测是否成功,是否需要重试 ============
            if((NumOfSplitFrames == file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath).size())&&!waifu2x_qprocess_failed)
            {
                break;
            }
            else
            {
                //失败
                file_DelDir(ScaledFramesFolderPath);
                file_mkDir(ScaledFramesFolderPath);
                if(retry==ui->spinBox_retry->value()+(ForceRetryCount-1))break;
                emit Send_TextBrowser_NewMessage(tr("Automatic retry, please wait."));
                Delay_sec_sleep(5);
            }
        }
        //===========
        //根据轮数修改参数
        DenoiseLevel_tmp = -1;
        if(ScaleRatio_Current_tmp<ScaleRatio_Max)
        {
            for(int x = 0; x < NumOfSplitFrames; x++)
            {
                QFile::remove(SplitFramesFolderPath+"/"+Frame_fileName_list.at(x));
            }
            file_MoveFiles_Folder_NcnnVulkanFolderProcess(ScaledFramesFolderPath, SplitFramesFolderPath, true);
            file_mkDir(ScaledFramesFolderPath);
        }
        CountFinishedRounds++;
    }
    emit Send_CurrentFileProgress_Stop();//停止当前文件进度条
    //======================= 检查是否成功放大所有帧 ===========================
    QStringList Frame_fileName_list_scaled = file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath);
    if(NumOfSplitFrames!=Frame_fileName_list_scaled.size())
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Failed to scale frames.]"));
        emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        file_DelDir(SplitFramesFolderPath);
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    //======================================== 组装 ======================================================
    if(CustRes_isEnabled)
    {
        ResGIFPath = file_path + "/" + file_name + "_waifu2x_"+QString::number(CustRes_width, 10)+"x"+QString::number(CustRes_height,10)+"_"+QString::number(DenoiseLevel, 10)+"n.gif";
    }
    Gif_assembleGif(ResGIFPath,ScaledFramesFolderPath,GIF_Duration,CustRes_isEnabled,CustRes_height,CustRes_width,isOverScaled,SourceFile_fullPath);
    if(!QFile::exists(ResGIFPath))
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to assemble gif.]"));
        emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        file_DelDir(SplitFramesFolderPath);
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    OutPutPath_Final = ResGIFPath;
    //======================================= 优化gif ===================================================
    if(OptGIF)
    {
        QString ResGIFPath_compressed = "";
        if(CustRes_isEnabled)
        {
            ResGIFPath_compressed = file_path + "/" + file_name + "_waifu2x_"+QString::number(CustRes_width, 10)+"x"+QString::number(CustRes_height,10)+"_"+QString::number(DenoiseLevel, 10)+"n_opt.gif";
        }
        else
        {
            ResGIFPath_compressed = file_path + "/" + file_name + "_waifu2x_"+QString::number(ScaleRatio, 10)+"x_"+QString::number(DenoiseLevel, 10)+"n_opt.gif";
        }
        OutPutPath_Final = Gif_compressGif(ResGIFPath,ResGIFPath_compressed);
    }
    //============================== 删除缓存文件 ====================================================
    file_DelDir(SplitFramesFolderPath);
    //============================= 删除原文件 & 更新table status ============================
    if(DelOriginal)
    {
        if(ReplaceOriginalFile(SourceFile_fullPath,OutPutPath_Final)==false)
        {
            if(QAction_checkBox_MoveToRecycleBin_checkBox_DelOriginal->isChecked())
            {
                file_MoveToTrash(SourceFile_fullPath);
            }
            else
            {
                QFile::remove(SourceFile_fullPath);
            }
        }
        emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Finished, original file deleted");
    }
    else
    {
        emit Send_Table_gif_ChangeStatus_rowNumInt_statusQString(rowNum, "Finished");
    }
    //========== 移动到输出路径 =========
    if(ui->checkBox_OutPath_isEnabled->isChecked())
    {
        MoveFileToOutputPath(OutPutPath_Final,SourceFile_fullPath);
    }
    //============================ 更新进度条 =================================
    emit Send_progressbar_Add();
    //===========================  ==============================
    return 0;
}

/*

  =============================================================================================
  =============================================================================================
  ============================= Waifu2x_NCNN_Vulkan_Video =======================================
  =============================================================================================
  =============================================================================================


*/

int MainWindow::Waifu2x_NCNN_Vulkan_Video(int rowNum)
{
    //============================= 读取设置 ================================
    int ScaleRatio = ui->doubleSpinBox_ScaleRatio_video->value();
    int DenoiseLevel = ui->spinBox_DenoiseLevel_video->value();
    bool DelOriginal = (ui->checkBox_DelOriginal->isChecked()||ui->checkBox_ReplaceOriginalFile->isChecked());
    bool isCacheExists = false;
    bool isVideoConfigChanged = true;
    //========================= 拆解map得到参数 =============================
    emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Processing");
    QString SourceFile_fullPath = Table_model_video->item(rowNum,2)->text();
    if(!QFile::exists(SourceFile_fullPath))
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [File does not exist.]"));
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;
    }
    //==========================
    QFileInfo fileinfo_SourceFile_fullPath(SourceFile_fullPath);
    QString SourceFile_fullPath_BaseName = file_getBaseName(SourceFile_fullPath);
    QString SourceFile_fullPath_suffix = fileinfo_SourceFile_fullPath.suffix();
    QString SourceFile_fullPath_FolderPath = file_getFolderPath(fileinfo_SourceFile_fullPath);
    //===================================================================
    //生成mp4
    QString video_mp4_fullpath=video_To_CFRMp4(SourceFile_fullPath);
    if(!QFile::exists(video_mp4_fullpath))//检查是否成功生成mp4
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Cannot convert video format to mp4.]"));
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    //=========== 读取自定义分辨率 ===============
    bool CustRes_isEnabled = false;
    int CustRes_height=0;
    int CustRes_width=0;
    if(CustRes_isContained(SourceFile_fullPath))
    {
        CustRes_isEnabled=true;
        QMap<QString, QString> Res_map = CustRes_getResMap(SourceFile_fullPath);//res_map["fullpath"],["height"],["width"]
        CustRes_height=Res_map["height"].toInt();
        CustRes_width=Res_map["width"].toInt();
    }
    //=================
    QString AudioPath = SourceFile_fullPath_FolderPath+"/Audio_"+SourceFile_fullPath_BaseName+"_"+SourceFile_fullPath_suffix+"_W2xEX.wav";//音频
    QString SplitFramesFolderPath = SourceFile_fullPath_FolderPath+"/"+SourceFile_fullPath_BaseName+"_"+SourceFile_fullPath_suffix+"_SplitFrames_W2xEX";//拆分后存储frame的文件夹
    QString ScaledFramesFolderPath = SplitFramesFolderPath+"/ScaledFrames_W2xEX";//存储放大后的帧
    //==========================
    //   检测之前的视频配置文件
    //==========================
    QString VideoConfiguration_fullPath = SourceFile_fullPath_FolderPath+"/VideoConfiguration_"+SourceFile_fullPath_BaseName+"_"+SourceFile_fullPath_suffix+"_Waifu2xEX.ini";
    if(QFile::exists(VideoConfiguration_fullPath))
    {
        QSettings *configIniRead = new QSettings(VideoConfiguration_fullPath, QSettings::IniFormat);
        configIniRead->setIniCodec(QTextCodec::codecForName("UTF-8"));
        //=================== 加载之前存储的视频信息 =========================
        int ScaleRatio_old = configIniRead->value("/VideoConfiguration/ScaleRatio").toInt();
        int DenoiseLevel_old = configIniRead->value("/VideoConfiguration/DenoiseLevel").toInt();
        bool CustRes_isEnabled_old = configIniRead->value("/VideoConfiguration/CustRes_isEnabled").toBool();
        int CustRes_height_old = configIniRead->value("/VideoConfiguration/CustRes_height").toInt();
        int CustRes_width_old = configIniRead->value("/VideoConfiguration/CustRes_width").toInt();
        QString EngineName_old = configIniRead->value("/VideoConfiguration/EngineName").toString();
        bool isProcessBySegment_old = configIniRead->value("/VideoConfiguration/isProcessBySegment").toBool();
        //=================== 比对信息 ================================
        if(EngineName_old=="waifu2x-ncnn-vulkan")
        {
            if(CustRes_isEnabled_old==false&&CustRes_isEnabled==false)
            {
                if(ScaleRatio_old!=ScaleRatio||DenoiseLevel_old!=DenoiseLevel)
                {
                    isVideoConfigChanged=true;
                }
                else
                {
                    isVideoConfigChanged=false;
                }
            }
            else
            {
                if(CustRes_isEnabled_old==true&&CustRes_isEnabled==true)
                {
                    if(CustRes_height_old!=CustRes_height||CustRes_width_old!=CustRes_width)
                    {
                        isVideoConfigChanged=true;
                    }
                    else
                    {
                        isVideoConfigChanged=false;
                    }
                }
                else
                {
                    isVideoConfigChanged=true;
                }
            }
            if(isProcessBySegment_old==true)
            {
                isVideoConfigChanged=true;
            }
        }
        else
        {
            isVideoConfigChanged=true;
        }
    }
    else
    {
        emit Send_video_write_VideoConfiguration(VideoConfiguration_fullPath,ScaleRatio,DenoiseLevel,CustRes_isEnabled,CustRes_height,CustRes_width,"waifu2x-ncnn-vulkan",false,"","",ui->groupBox_FrameInterpolation->isChecked(),0);
    }
    //=======================
    //   检测缓存是否存在
    //=======================
    if(QFile::exists(video_mp4_fullpath)&&file_isDirExist(SplitFramesFolderPath)&&file_isDirExist(ScaledFramesFolderPath)&&QFile::exists(VideoConfiguration_fullPath))
    {
        if(!isVideoConfigChanged)
        {
            isCacheExists=true;
            emit Send_TextBrowser_NewMessage(tr("The previous video cache file is detected and processing of the previous video cache will continue. If you want to restart processing of the current video:[")+SourceFile_fullPath+tr("], delete the cache manually."));
        }
        else
        {
            isCacheExists=false;
            //========
            QFile::remove(VideoConfiguration_fullPath);
            file_DelDir(SplitFramesFolderPath);
            QFile::remove(AudioPath);
            DelVfiDir(video_mp4_fullpath);
            emit Send_video_write_VideoConfiguration(VideoConfiguration_fullPath,ScaleRatio,DenoiseLevel,CustRes_isEnabled,CustRes_height,CustRes_width,"waifu2x-ncnn-vulkan",false,"","",ui->groupBox_FrameInterpolation->isChecked(),0);
            //=======
            emit Send_TextBrowser_NewMessage(tr("The previous video cache file was detected, but because you changed the settings about the video resolution or denoise level, the previous cache will be deleted and processing of the video:[")+SourceFile_fullPath+tr("] will restart."));
        }
    }
    else
    {
        isCacheExists=false;
        //========
        QFile::remove(VideoConfiguration_fullPath);
        file_DelDir(SplitFramesFolderPath);
        QFile::remove(AudioPath);
        DelVfiDir(video_mp4_fullpath);
        emit Send_video_write_VideoConfiguration(VideoConfiguration_fullPath,ScaleRatio,DenoiseLevel,CustRes_isEnabled,CustRes_height,CustRes_width,"waifu2x-ncnn-vulkan",false,"","",ui->groupBox_FrameInterpolation->isChecked(),0);
        //========
    }
    if(!isCacheExists)
    {
        //============================== 拆分 ==========================================
        if(file_isDirExist(SplitFramesFolderPath))
        {
            file_DelDir(SplitFramesFolderPath);
            file_mkDir(SplitFramesFolderPath);
        }
        else
        {
            file_mkDir(SplitFramesFolderPath);
        }
        QFile::remove(AudioPath);
        video_video2images(video_mp4_fullpath,SplitFramesFolderPath,AudioPath);
    }
    //============================== 扫描获取文件名 ===============================
    QStringList Frame_fileName_list = file_getFileNames_in_Folder_nofilter(SplitFramesFolderPath);
    if(!isCacheExists)
    {
        if(Frame_fileName_list.isEmpty())//检查是否成功拆分为帧
        {
            emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to split video into pictures.]"));
            emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
            emit Send_progressbar_Add();
            return 0;//如果启用stop位,则直接return
        }
    }
    //============================== 放大 =======================================
    //===========建立存储放大后frame的文件夹===========
    if(!isCacheExists)
    {
        if(file_isDirExist(ScaledFramesFolderPath))
        {
            file_DelDir(ScaledFramesFolderPath);
            file_mkDir(ScaledFramesFolderPath);
        }
        else
        {
            file_mkDir(ScaledFramesFolderPath);
        }
    }
    //==========开始放大==========================
    //读取放大倍数
    int ScaleRatio_Original = 2;
    if(CustRes_isEnabled == true)
    {
        ScaleRatio_Original = CustRes_CalNewScaleRatio(video_mp4_fullpath,CustRes_height,CustRes_width);
    }
    else
    {
        ScaleRatio_Original = ui->doubleSpinBox_ScaleRatio_video->value();
    }
    int ScaleRatio_Max=Calculate_Temporary_ScaleRatio_W2xNCNNVulkan(ScaleRatio_Original);
    bool isOverScaled = (ScaleRatio_Max!=ScaleRatio_Original);
    //读取初始放大倍率
    int Initial_ScaleRatio = 2;
    if(!Frame_fileName_list.isEmpty())
    {
        QString Full_Path_File = "";
        for(int i = 0; i < Frame_fileName_list.size(); i++)
        {
            QString tmp = Frame_fileName_list.at(i);
            Full_Path_File = SplitFramesFolderPath + "/" + tmp;
            QFileInfo finfo(Full_Path_File);
            if(finfo.suffix()=="png")break;
        }
        QImage qimage_original;
        qimage_original.load(Full_Path_File);
        int Frame_height = qimage_original.height();
        QMap<QString,int> res_map = video_get_Resolution(SourceFile_fullPath);
        int Video_height = res_map["height"];
        if(Frame_height>0 && Video_height>0 && Frame_height>=Video_height)//判断是否读取失败
        {
            Initial_ScaleRatio = 2*(Frame_height/Video_height);
        }
    }
    //初始化进度讯息
    int NumOfSplitFrames = Frame_fileName_list.size()+file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath).size();
    if(ui->checkBox_ShowInterPro->isChecked())
    {
        int NumOfRounds=0;
        for(int ScaleRatio_Current_tmp=Initial_ScaleRatio; ScaleRatio_Current_tmp<=ScaleRatio_Max; ScaleRatio_Current_tmp*=2)
        {
            NumOfRounds++;
        }
        emit Send_CurrentFileProgress_Start(SourceFile_fullPath_BaseName+"."+SourceFile_fullPath_suffix,NumOfSplitFrames*NumOfRounds);
    }
    //读取配置讯息
    QString Waifu2x_NCNN_Vulkan_Settings_str = "";
    QString cmd="";
    QProcess *Waifu2x = new QProcess();
    QString program = Waifu2x_ncnn_vulkan_ProgramPath;
    bool waifu2x_qprocess_failed = false;
    int DenoiseLevel_tmp = DenoiseLevel;
    DenoiseLevel_tmp = DenoiseLevel;
    int CountFinishedRounds=0;
    //====
    for(int ScaleRatio_Current_tmp=Initial_ScaleRatio; ScaleRatio_Current_tmp<=ScaleRatio_Max && Frame_fileName_list.isEmpty()==false; ScaleRatio_Current_tmp*=2)
    {
        for(int retry=0; retry<(ui->spinBox_retry->value()+ForceRetryCount); retry++)
        {
            QString ErrorMSG="";
            QString StanderMSG="";
            //==========
            waifu2x_qprocess_failed = false;
            Waifu2x_NCNN_Vulkan_Settings_str = Waifu2x_NCNN_Vulkan_ReadSettings_Video_GIF(ui->spinBox_ThreadNum_video_internal->value());
            cmd = "\"" + program + "\"" + " -i " + "\"" + SplitFramesFolderPath + "\"" + " -o " + "\"" + ScaledFramesFolderPath + "\"" + " -s " + "2" + " -n " + QString::number(DenoiseLevel_tmp, 10) + Waifu2x_NCNN_Vulkan_Settings_str;
            Waifu2x->start(cmd);
            while(!Waifu2x->waitForStarted(100)&&!QProcess_stop) {}
            while(!Waifu2x->waitForFinished(650)&&!QProcess_stop)
            {
                if(waifu2x_STOP)
                {
                    QStringList FinfishedFileList_FileNames = WaitForEngineIO_NcnnVulkan(ScaledFramesFolderPath);
                    Waifu2x->close();
                    //============
                    QString file_name_tmp;
                    QStringList FileNames_ScaledFramesFolderPath = file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath);
                    for(int i=0; i<FileNames_ScaledFramesFolderPath.size(); i++)
                    {
                        file_name_tmp = FileNames_ScaledFramesFolderPath.at(i);
                        if(FinfishedFileList_FileNames.contains(file_name_tmp))
                        {
                            QFile::remove(SplitFramesFolderPath + "/" + file_name_tmp);
                        }
                        else
                        {
                            QFile::remove(ScaledFramesFolderPath + "/" + file_name_tmp);
                        }
                    }
                    //=============
                    emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Interrupted");
                    mutex_ThreadNumRunning.lock();
                    ThreadNumRunning--;
                    mutex_ThreadNumRunning.unlock();
                    return 0;
                }
                ErrorMSG.append(Waifu2x->readAllStandardError().toLower());
                StanderMSG.append(Waifu2x->readAllStandardOutput().toLower());
                if(ErrorMSG.contains("failed")||StanderMSG.contains("failed"))
                {
                    waifu2x_qprocess_failed = true;
                    Waifu2x->close();
                    break;
                }
                if(ui->checkBox_ShowInterPro->isChecked())
                {
                    emit Send_CurrentFileProgress_progressbar_SetFinishedValue((NumOfSplitFrames*CountFinishedRounds)+file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath).size());
                }
            }
            //===============
            if(waifu2x_qprocess_failed==false)
            {
                ErrorMSG.append(Waifu2x->readAllStandardError().toLower());
                StanderMSG.append(Waifu2x->readAllStandardOutput().toLower());
                if(ErrorMSG.contains("failed")||StanderMSG.contains("failed"))
                {
                    waifu2x_qprocess_failed = true;
                }
            }
            //========= 检测是否成功,是否需要重试 ============
            if((NumOfSplitFrames == file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath).size())&&!waifu2x_qprocess_failed)
            {
                break;
            }
            else
            {
                //失败
                file_DelDir(ScaledFramesFolderPath);
                file_mkDir(ScaledFramesFolderPath);
                if(retry==ui->spinBox_retry->value()+(ForceRetryCount-1))break;
                emit Send_TextBrowser_NewMessage(tr("Automatic retry, please wait."));
                Delay_sec_sleep(5);
            }
        }
        //===========
        //根据轮数修改参数
        DenoiseLevel_tmp = -1;
        if(ScaleRatio_Current_tmp<ScaleRatio_Max)
        {
            QStringList SplitFolderList = file_getFileNames_in_Folder_nofilter(SplitFramesFolderPath);
            for(int x = 0; x < SplitFolderList.size(); x++)
            {
                QFile::remove(SplitFramesFolderPath+"/"+SplitFolderList.at(x));
            }
            file_MoveFiles_Folder_NcnnVulkanFolderProcess(ScaledFramesFolderPath, SplitFramesFolderPath, true);
            file_mkDir(ScaledFramesFolderPath);
        }
        CountFinishedRounds++;
    }
    emit Send_CurrentFileProgress_Stop();//停止当前文件进度条
    //================ 扫描放大后的帧文件数量,判断是否放大成功 =======================
    QStringList Frame_fileName_list_scaled = file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath);
    if(Frame_fileName_list_scaled.size()<NumOfSplitFrames)
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to scale all frames.]"));
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    //======================================== 组装 ======================================================
    QString video_mp4_scaled_fullpath = "";
    if(CustRes_isEnabled)
    {
        video_mp4_scaled_fullpath = SourceFile_fullPath_FolderPath+"/"+SourceFile_fullPath_BaseName+"_waifu2x_"+QString::number(CustRes_width,10)+"x"+QString::number(CustRes_height,10)+"_"+QString::number(DenoiseLevel,10)+"n"+"_"+SourceFile_fullPath_suffix+".mp4";
    }
    else
    {
        video_mp4_scaled_fullpath = SourceFile_fullPath_FolderPath+"/"+SourceFile_fullPath_BaseName+"_waifu2x_"+QString::number(ScaleRatio,10)+"x_"+QString::number(DenoiseLevel,10)+"n"+"_"+SourceFile_fullPath_suffix+".mp4";
    }
    video_images2video(video_mp4_fullpath,video_mp4_scaled_fullpath,ScaledFramesFolderPath,AudioPath,CustRes_isEnabled,CustRes_height,CustRes_width,isOverScaled);
    if(QFile::exists(video_mp4_scaled_fullpath)==false)//检查是否成功成功生成视频
    {
        if(waifu2x_STOP)
        {
            emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Interrupted");
            return 0;//如果启用stop位,则直接return
        }
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to assemble pictures into videos.]"));
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    //============================== 删除缓存文件 ====================================================
    if(ui->checkBox_KeepVideoCache->isChecked()==false)
    {
        QFile::remove(VideoConfiguration_fullPath);
        file_DelDir(SplitFramesFolderPath);
        QFile::remove(AudioPath);
        if(SourceFile_fullPath!=video_mp4_fullpath)QFile::remove(video_mp4_fullpath);
    }
    else
    {
        DelOriginal=false;
    }
    //============================= 删除原文件 &  & 更新table status ============================
    if(DelOriginal)
    {
        if(ReplaceOriginalFile(SourceFile_fullPath,video_mp4_scaled_fullpath)==false)
        {
            if(QAction_checkBox_MoveToRecycleBin_checkBox_DelOriginal->isChecked())
            {
                file_MoveToTrash(SourceFile_fullPath);
            }
            else
            {
                QFile::remove(SourceFile_fullPath);
            }
        }
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Finished, original file deleted");
    }
    else
    {
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Finished");
    }
    //========== 移动到输出路径 =========
    if(ui->checkBox_OutPath_isEnabled->isChecked())
    {
        MoveFileToOutputPath(video_mp4_scaled_fullpath,SourceFile_fullPath);
    }
    //============================ 更新进度条 =================================
    emit Send_progressbar_Add();
    //===========================  ==============================
    return 0;
}
/*
#### 分段处理 ####
调用Waifu2x-NCNN-Vulkan处理视频
读取设置,拆分,创建放大子线程,组装视频
*/
int MainWindow::Waifu2x_NCNN_Vulkan_Video_BySegment(int rowNum)
{
    //============================= 读取设置 ================================
    int ScaleRatio = ui->doubleSpinBox_ScaleRatio_video->value();
    int DenoiseLevel = ui->spinBox_DenoiseLevel_video->value();
    bool DelOriginal = (ui->checkBox_DelOriginal->isChecked()||ui->checkBox_ReplaceOriginalFile->isChecked());
    bool isCacheExists = false;
    bool isVideoConfigChanged = true;
    int SegmentDuration = ui->spinBox_SegmentDuration->value();
    //========================= 拆解map得到参数 =============================
    emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Processing");
    QString SourceFile_fullPath = Table_model_video->item(rowNum,2)->text();
    if(!QFile::exists(SourceFile_fullPath))
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [File does not exist.]"));
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;
    }
    //==========================
    QFileInfo fileinfo(SourceFile_fullPath);
    QString file_name = file_getBaseName(SourceFile_fullPath);
    QString file_ext = fileinfo.suffix();
    QString file_path = file_getFolderPath(fileinfo);
    //===================================================================
    //生成mp4
    QString video_mp4_fullpath=video_To_CFRMp4(SourceFile_fullPath);
    if(!QFile::exists(video_mp4_fullpath))//检查是否成功生成mp4
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Cannot convert video format to mp4.]"));
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    //============ 读取自定义分辨率 ==============
    bool CustRes_isEnabled = false;
    int CustRes_height=0;
    int CustRes_width=0;
    if(CustRes_isContained(SourceFile_fullPath))
    {
        CustRes_isEnabled=true;
        QMap<QString, QString> Res_map = CustRes_getResMap(SourceFile_fullPath);//res_map["fullpath"],["height"],["width"]
        CustRes_height=Res_map["height"].toInt();
        CustRes_width=Res_map["width"].toInt();
    }
    //=================
    QString AudioPath = file_path+"/Audio_"+file_name+"_"+file_ext+"_W2xEX.wav";//音频
    QString SplitFramesFolderPath = file_path+"/"+file_name+"_"+file_ext+"_SplitFrames_W2xEX";//拆分后存储frame的文件夹
    QString ScaledFramesFolderPath = SplitFramesFolderPath+"/ScaledFrames_W2xEX";//存储放大后的帧
    //===
    QString VideoClipsFolderPath = "";//存储视频片段的文件夹(完整路径)
    QString DateStr = "";
    do
    {
        DateStr = video_getClipsFolderNo();
        VideoClipsFolderPath = file_path+"/"+DateStr+"_VideoClipsWaifu2xEX";//存储视频片段的文件夹(完整路径)
    }
    while(file_isDirExist(VideoClipsFolderPath));
    QString VideoClipsFolderName = DateStr+"_VideoClipsWaifu2xEX";//存储视频片段的文件夹(名称)
    //==========================
    //   检测之前的视频配置文件
    //==========================
    QString VideoConfiguration_fullPath = file_path+"/VideoConfiguration_"+file_name+"_"+file_ext+"_Waifu2xEX.ini";
    if(QFile::exists(VideoConfiguration_fullPath))
    {
        QSettings *configIniRead = new QSettings(VideoConfiguration_fullPath, QSettings::IniFormat);
        configIniRead->setIniCodec(QTextCodec::codecForName("UTF-8"));
        //=================== 加载之前存储的视频信息 =========================
        int ScaleRatio_old = configIniRead->value("/VideoConfiguration/ScaleRatio").toInt();
        int DenoiseLevel_old = configIniRead->value("/VideoConfiguration/DenoiseLevel").toInt();
        bool CustRes_isEnabled_old = configIniRead->value("/VideoConfiguration/CustRes_isEnabled").toBool();
        int CustRes_height_old = configIniRead->value("/VideoConfiguration/CustRes_height").toInt();
        int CustRes_width_old = configIniRead->value("/VideoConfiguration/CustRes_width").toInt();
        QString EngineName_old = configIniRead->value("/VideoConfiguration/EngineName").toString();
        bool isProcessBySegment_old = configIniRead->value("/VideoConfiguration/isProcessBySegment").toBool();
        bool isVideoFrameInterpolationEnabled_old = configIniRead->value("/VideoConfiguration/isVideoFrameInterpolationEnabled").toBool();
        int MultipleOfFPS_old = configIniRead->value("/VideoConfiguration/MultipleOfFPS_old").toInt();
        //=================== 比对信息 ================================
        if(EngineName_old=="waifu2x-ncnn-vulkan")
        {
            if(CustRes_isEnabled_old==false&&CustRes_isEnabled==false)
            {
                if(ScaleRatio_old!=ScaleRatio||DenoiseLevel_old!=DenoiseLevel)
                {
                    isVideoConfigChanged=true;
                }
                else
                {
                    isVideoConfigChanged=false;
                }
            }
            else
            {
                if(CustRes_isEnabled_old==true&&CustRes_isEnabled==true)
                {
                    if(CustRes_height_old!=CustRes_height||CustRes_width_old!=CustRes_width)
                    {
                        isVideoConfigChanged=true;
                    }
                    else
                    {
                        isVideoConfigChanged=false;
                    }
                }
                else
                {
                    isVideoConfigChanged=true;
                }
            }
            if(isProcessBySegment_old == false)
            {
                isVideoConfigChanged=true;
            }
            if(isVideoFrameInterpolationEnabled_old != ui->groupBox_FrameInterpolation->isChecked())
            {
                isVideoConfigChanged=true;
            }
            if(MultipleOfFPS_old != ui->spinBox_MultipleOfFPS_VFI->value() && ui->groupBox_FrameInterpolation->isChecked())
            {
                isVideoConfigChanged=true;
            }
        }
        else
        {
            isVideoConfigChanged=true;
        }
        //============ 修正文件夹名称 =============
        QString VideoClipsFolderPath_old = configIniRead->value("/VideoConfiguration/VideoClipsFolderPath").toString();
        QString VideoClipsFolderName_old = configIniRead->value("/VideoConfiguration/VideoClipsFolderName").toString();
        file_mkDir(VideoClipsFolderPath_old);
        if(file_isDirExist(VideoClipsFolderPath_old)==true)
        {
            VideoClipsFolderPath = VideoClipsFolderPath_old;
            VideoClipsFolderName = VideoClipsFolderName_old;
        }
    }
    else
    {
        emit Send_video_write_VideoConfiguration(VideoConfiguration_fullPath,ScaleRatio,DenoiseLevel,CustRes_isEnabled,CustRes_height,CustRes_width,"waifu2x-ncnn-vulkan",true,VideoClipsFolderPath,VideoClipsFolderName,ui->groupBox_FrameInterpolation->isChecked(),ui->spinBox_MultipleOfFPS_VFI->value());
    }
    //=======================
    //   检测缓存是否存在
    //=======================
    if(file_isDirExist(SplitFramesFolderPath))
    {
        if(!isVideoConfigChanged)
        {
            isCacheExists=true;
            emit Send_TextBrowser_NewMessage(tr("The previous video cache file is detected and processing of the previous video cache will continue. If you want to restart processing of the current video:[")+SourceFile_fullPath+tr("], delete the cache manually."));
        }
        else
        {
            isCacheExists=false;
            //========
            QFile::remove(VideoConfiguration_fullPath);
            file_DelDir(SplitFramesFolderPath);
            file_DelDir(VideoClipsFolderPath);
            QFile::remove(AudioPath);
            DelVfiDir(video_mp4_fullpath);
            emit Send_video_write_VideoConfiguration(VideoConfiguration_fullPath,ScaleRatio,DenoiseLevel,CustRes_isEnabled,CustRes_height,CustRes_width,"waifu2x-ncnn-vulkan",true,VideoClipsFolderPath,VideoClipsFolderName,ui->groupBox_FrameInterpolation->isChecked(),ui->spinBox_MultipleOfFPS_VFI->value());
            //=======
            emit Send_TextBrowser_NewMessage(tr("The previous video cache file was detected, but because you changed the settings about the video resolution or denoise level, the previous cache will be deleted and processing of the video:[")+SourceFile_fullPath+tr("] will restart."));
        }
    }
    else
    {
        isCacheExists=false;
        //========
        QFile::remove(VideoConfiguration_fullPath);
        file_DelDir(SplitFramesFolderPath);
        file_DelDir(VideoClipsFolderPath);
        QFile::remove(AudioPath);
        DelVfiDir(video_mp4_fullpath);
        emit Send_video_write_VideoConfiguration(VideoConfiguration_fullPath,ScaleRatio,DenoiseLevel,CustRes_isEnabled,CustRes_height,CustRes_width,"waifu2x-ncnn-vulkan",true,VideoClipsFolderPath,VideoClipsFolderName,ui->groupBox_FrameInterpolation->isChecked(),ui->spinBox_MultipleOfFPS_VFI->value());
        //========
    }
    /*====================================
                  提取音频
    ======================================*/
    if(!QFile::exists(AudioPath))
    {
        video_get_audio(video_mp4_fullpath,AudioPath);
    }
    //================================== 开始分段处理视频 =================================================
    int StartTime = 0;//起始时间(秒)
    int VideoDuration = video_get_duration(video_mp4_fullpath);
    bool isSplitComplete = false;
    bool isScaleComplete = false;
    /*
    ============================================
                  开始之前先读取进度
    ============================================
    */
    int OLD_SegmentDuration=-1;
    bool read_OLD_SegmentDuration =false;
    int LastVideoClipNo = -1;
    if(QFile::exists(VideoConfiguration_fullPath))
    {
        QSettings *configIniRead = new QSettings(VideoConfiguration_fullPath, QSettings::IniFormat);
        configIniRead->setIniCodec(QTextCodec::codecForName("UTF-8"));
        //=================== 加载进度 =========================
        StartTime = configIniRead->value("/Progress/StartTime").toInt();
        isSplitComplete = configIniRead->value("/Progress/isSplitComplete").toBool();
        isScaleComplete = configIniRead->value("/Progress/isScaleComplete").toBool();
        OLD_SegmentDuration = configIniRead->value("/Progress/OLDSegmentDuration").toInt();
        LastVideoClipNo = configIniRead->value("/Progress/LastVideoClipNo").toInt();
    }
    if(OLD_SegmentDuration>0)
    {
        read_OLD_SegmentDuration = true;
    }
    /*
    加载进度条
    */
    int SegmentDuration_tmp_progressbar = 0;
    if(read_OLD_SegmentDuration)
    {
        SegmentDuration_tmp_progressbar = OLD_SegmentDuration;
    }
    else
    {
        SegmentDuration_tmp_progressbar = SegmentDuration;
    }
    if(ui->checkBox_ShowInterPro->isChecked()&&VideoDuration>SegmentDuration_tmp_progressbar)
    {
        emit Send_CurrentFileProgress_Start(file_name+"."+file_ext,VideoDuration);
        if(StartTime>0)
        {
            emit Send_CurrentFileProgress_progressbar_Add_SegmentDuration(StartTime);
        }
    }
    /*
    ============================================
                    正式开始处理
    ============================================
    */
    int SegmentDuration_tmp=0;
    int TimeLeft_tmp=0;
    bool isOverScaled = false;
    QString video_mp4_scaled_clip_fullpath="";
    //==========================
    while(VideoDuration>StartTime)
    {
        /*==========================
                计算视频片段时间
        ==========================*/
        TimeLeft_tmp = VideoDuration-StartTime;
        if((TimeLeft_tmp)>=SegmentDuration)
        {
            SegmentDuration_tmp = SegmentDuration;
        }
        else
        {
            SegmentDuration_tmp = TimeLeft_tmp;
        }
        if(read_OLD_SegmentDuration)
        {
            SegmentDuration_tmp = OLD_SegmentDuration;
            read_OLD_SegmentDuration=false;
        }
        /*==========================
                 拆分视频片段
        ==========================*/
        if(isSplitComplete==false)
        {
            if(file_isDirExist(SplitFramesFolderPath))
            {
                file_DelDir(SplitFramesFolderPath);
                file_mkDir(SplitFramesFolderPath);
            }
            else
            {
                file_mkDir(SplitFramesFolderPath);
            }
            video_video2images_ProcessBySegment(video_mp4_fullpath,SplitFramesFolderPath,StartTime,SegmentDuration_tmp);
        }
        /*==========================
               处理视频片段的帧
        ==========================*/
        isOverScaled = false;
        if(isScaleComplete==false)
        {
            //============================== 扫描获取文件名 ===============================
            QStringList Frame_fileName_list = file_getFileNames_in_Folder_nofilter(SplitFramesFolderPath);
            if(isSplitComplete==false)
            {
                if(Frame_fileName_list.isEmpty())//检查是否成功拆分为帧
                {
                    emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to split video into pictures.]"));
                    emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
                    emit Send_progressbar_Add();
                    return 0;//如果启用stop位,则直接return
                }
            }
            /*
            记录进度
            帧拆分成功
            */
            emit Send_video_write_Progress_ProcessBySegment(VideoConfiguration_fullPath,StartTime,true,false,SegmentDuration_tmp,LastVideoClipNo);
            //============================== 放大 =======================================
            //===========建立存储放大后frame的文件夹===========
            if(isSplitComplete==false)
            {
                if(file_isDirExist(ScaledFramesFolderPath))
                {
                    file_DelDir(ScaledFramesFolderPath);
                    file_mkDir(ScaledFramesFolderPath);
                }
                else
                {
                    file_mkDir(ScaledFramesFolderPath);
                }
            }
            //==========开始放大==========================
            //读取放大倍数
            int ScaleRatio_Original = 2;
            if(CustRes_isEnabled == true)
            {
                ScaleRatio_Original = CustRes_CalNewScaleRatio(video_mp4_fullpath,CustRes_height,CustRes_width);
            }
            else
            {
                ScaleRatio_Original = ui->doubleSpinBox_ScaleRatio_video->value();
            }
            int ScaleRatio_Max=Calculate_Temporary_ScaleRatio_W2xNCNNVulkan(ScaleRatio_Original);
            isOverScaled = (ScaleRatio_Max!=ScaleRatio_Original);
            //读取初始放大倍率
            int Initial_ScaleRatio = 2;
            if(!Frame_fileName_list.isEmpty())
            {
                QString Full_Path_File = "";
                for(int i = 0; i < Frame_fileName_list.size(); i++)
                {
                    QString tmp = Frame_fileName_list.at(i);
                    Full_Path_File = SplitFramesFolderPath + "/" + tmp;
                    QFileInfo finfo(Full_Path_File);
                    if(finfo.suffix()=="png")break;
                }
                QImage qimage_original;
                qimage_original.load(Full_Path_File);
                int Frame_height = qimage_original.height();
                QMap<QString,int> res_map = video_get_Resolution(SourceFile_fullPath);
                int Video_height = res_map["height"];
                if(Frame_height>0 && Video_height>0 && Frame_height>=Video_height)//判断是否读取失败
                {
                    Initial_ScaleRatio = 2*(Frame_height/Video_height);
                }
            }
            //初始化进度讯息
            int NumOfSplitFrames = Frame_fileName_list.size()+file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath).size();
            int CurrentFileProgress_MAX = 0;
            int CurrentFileProgress_OLD = 0;
            int CurrentFileProgress_New = 0;
            if(ui->checkBox_ShowInterPro->isChecked())
            {
                int NumOfRounds=0;
                for(int ScaleRatio_Current_tmp=Initial_ScaleRatio; ScaleRatio_Current_tmp<=ScaleRatio_Max; ScaleRatio_Current_tmp*=2)
                {
                    NumOfRounds++;
                }
                CurrentFileProgress_MAX =NumOfSplitFrames*NumOfRounds;
            }
            //读取配置讯息
            QProcess *Waifu2x = new QProcess();
            bool waifu2x_qprocess_failed = false;
            int DenoiseLevel_tmp = DenoiseLevel;
            int CountFinishedRounds=0;
            QString ErrorMSG="";
            QString StanderMSG="";
            QString cmd="";
            QString Waifu2x_NCNN_Vulkan_Settings_str="";
            //====
            for(int ScaleRatio_Current_tmp=Initial_ScaleRatio; ScaleRatio_Current_tmp<=ScaleRatio_Max && Frame_fileName_list.isEmpty()==false; ScaleRatio_Current_tmp*=2)
            {
                for(int retry=0; retry<(ui->spinBox_retry->value()+ForceRetryCount); retry++)
                {
                    ErrorMSG="";
                    StanderMSG="";
                    //==========
                    waifu2x_qprocess_failed = false;
                    Waifu2x_NCNN_Vulkan_Settings_str = Waifu2x_NCNN_Vulkan_ReadSettings_Video_GIF(ui->spinBox_ThreadNum_video_internal->value());
                    cmd = "\"" + Waifu2x_ncnn_vulkan_ProgramPath + "\"" + " -i " + "\"" + SplitFramesFolderPath + "\"" + " -o " + "\"" + ScaledFramesFolderPath + "\"" + " -s " + "2" + " -n " + QString::number(DenoiseLevel_tmp, 10) + Waifu2x_NCNN_Vulkan_Settings_str;
                    Waifu2x->start(cmd);
                    while(!Waifu2x->waitForStarted(100)&&!QProcess_stop) {}
                    while(!Waifu2x->waitForFinished(650)&&!QProcess_stop)
                    {
                        if(waifu2x_STOP)
                        {
                            QStringList FinfishedFileList_FileNames = WaitForEngineIO_NcnnVulkan(ScaledFramesFolderPath);
                            Waifu2x->close();
                            //============
                            QString file_name_tmp;
                            QStringList FileNames_ScaledFramesFolderPath = file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath);
                            for(int i=0; i<FileNames_ScaledFramesFolderPath.size(); i++)
                            {
                                file_name_tmp = FileNames_ScaledFramesFolderPath.at(i);
                                if(FinfishedFileList_FileNames.contains(file_name_tmp))
                                {
                                    QFile::remove(SplitFramesFolderPath + "/" + file_name_tmp);
                                }
                                else
                                {
                                    QFile::remove(ScaledFramesFolderPath + "/" + file_name_tmp);
                                }
                            }
                            //=============
                            emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Interrupted");
                            mutex_ThreadNumRunning.lock();
                            ThreadNumRunning--;
                            mutex_ThreadNumRunning.unlock();
                            return 0;
                        }
                        ErrorMSG.append(Waifu2x->readAllStandardError().toLower());
                        StanderMSG.append(Waifu2x->readAllStandardOutput().toLower());
                        if(ErrorMSG.contains("failed")||StanderMSG.contains("failed"))
                        {
                            waifu2x_qprocess_failed = true;
                            Waifu2x->close();
                            break;
                        }
                        if(ui->checkBox_ShowInterPro->isChecked())
                        {
                            CurrentFileProgress_New = (NumOfSplitFrames*CountFinishedRounds)+file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath).size();
                            if(CurrentFileProgress_New!=CurrentFileProgress_OLD)
                            {
                                emit Send_TextBrowser_NewMessage(tr("File name:[")+SourceFile_fullPath+tr("]  Scale progress:[")+QString::number(CurrentFileProgress_New,10)+"/"+QString::number(CurrentFileProgress_MAX,10)+tr("] Duration progress:[")+QString::number(StartTime,10)+"s/"+QString::number(VideoDuration,10)+"s]");
                                CurrentFileProgress_OLD=CurrentFileProgress_New;
                            }
                        }
                    }
                    //===============
                    if(waifu2x_qprocess_failed==false)
                    {
                        ErrorMSG.append(Waifu2x->readAllStandardError().toLower());
                        StanderMSG.append(Waifu2x->readAllStandardOutput().toLower());
                        if(ErrorMSG.contains("failed")||StanderMSG.contains("failed"))
                        {
                            waifu2x_qprocess_failed = true;
                        }
                    }
                    //========= 检测是否成功,是否需要重试 ============
                    if((NumOfSplitFrames == file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath).size())&&!waifu2x_qprocess_failed)
                    {
                        break;
                    }
                    else
                    {
                        //失败
                        file_DelDir(ScaledFramesFolderPath);
                        file_mkDir(ScaledFramesFolderPath);
                        if(retry==ui->spinBox_retry->value()+(ForceRetryCount-1))break;
                        emit Send_TextBrowser_NewMessage(tr("Automatic retry, please wait."));
                        Delay_sec_sleep(5);
                    }
                }
                //===========
                //根据轮数修改参数
                DenoiseLevel_tmp = -1;
                if(ScaleRatio_Current_tmp<ScaleRatio_Max)
                {
                    QStringList SplitFolderList = file_getFileNames_in_Folder_nofilter(SplitFramesFolderPath);
                    for(int x = 0; x < SplitFolderList.size(); x++)
                    {
                        QFile::remove(SplitFramesFolderPath+"/"+SplitFolderList.at(x));
                    }
                    file_MoveFiles_Folder_NcnnVulkanFolderProcess(ScaledFramesFolderPath, SplitFramesFolderPath, true);
                    file_mkDir(ScaledFramesFolderPath);
                }
                CountFinishedRounds++;
            }
            //================ 扫描放大后的帧文件数量,判断是否放大成功 =======================
            QStringList Frame_fileName_list_scaled = file_getFileNames_in_Folder_nofilter(ScaledFramesFolderPath);
            if(Frame_fileName_list_scaled.size()<NumOfSplitFrames)
            {
                emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to scale all frames.]"));
                emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
                emit Send_progressbar_Add();
                return 0;//如果启用stop位,则直接return
            }
            /*
            记录进度
            帧处理成功
            */
            emit Send_video_write_Progress_ProcessBySegment(VideoConfiguration_fullPath,StartTime,true,true,SegmentDuration_tmp,LastVideoClipNo);
        }
        /*==========================
            组装视频片段(由帧到视频)
        ==========================*/
        if(!file_isDirExist(VideoClipsFolderPath))
        {
            file_mkDir(VideoClipsFolderPath);
        }
        int VideoClipNo = LastVideoClipNo+1;
        video_mp4_scaled_clip_fullpath = VideoClipsFolderPath+"/"+QString::number(VideoClipNo,10)+".mp4";
        video_images2video(video_mp4_fullpath,video_mp4_scaled_clip_fullpath,ScaledFramesFolderPath,"",CustRes_isEnabled,CustRes_height,CustRes_width,isOverScaled);
        if(!QFile::exists(video_mp4_scaled_clip_fullpath))//检查是否成功成功生成视频
        {
            if(waifu2x_STOP)
            {
                emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Interrupted");
                return 0;//如果启用stop位,则直接return
            }
            emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to assemble pictures into videos.]"));
            emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
            emit Send_progressbar_Add();
            return 0;//如果启用stop位,则直接return
        }
        /*==========================
              处理完当前片段
        ==========================*/
        if(ui->checkBox_ShowInterPro->isChecked())
        {
            emit Send_CurrentFileProgress_progressbar_Add_SegmentDuration(SegmentDuration_tmp);
        }
        StartTime+=SegmentDuration_tmp;
        isSplitComplete = false;
        isScaleComplete = false;
        LastVideoClipNo=VideoClipNo;
        emit Send_video_write_Progress_ProcessBySegment(VideoConfiguration_fullPath,StartTime,false,false,-1,VideoClipNo);
    }
    emit Send_CurrentFileProgress_Stop();
    //======================================================
    //                    组装(片段到成片)
    //======================================================
    QString video_mp4_scaled_fullpath = "";
    if(CustRes_isEnabled)
    {
        video_mp4_scaled_fullpath = file_path+"/"+file_name+"_waifu2x_"+QString::number(CustRes_width,10)+"x"+QString::number(CustRes_height,10)+"_"+QString::number(DenoiseLevel,10)+"n"+"_"+file_ext+".mp4";
    }
    else
    {
        video_mp4_scaled_fullpath = file_path+"/"+file_name+"_waifu2x_"+QString::number(ScaleRatio,10)+"x_"+QString::number(DenoiseLevel,10)+"n"+"_"+file_ext+".mp4";
    }
    QFile::remove(video_mp4_scaled_fullpath);
    video_AssembleVideoClips(VideoClipsFolderPath,VideoClipsFolderName,video_mp4_scaled_fullpath,AudioPath);
    if(QFile::exists(video_mp4_scaled_fullpath)==false)//检查是否成功生成视频
    {
        if(waifu2x_STOP)
        {
            emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Interrupted");
            return 0;//如果启用stop位,则直接return
        }
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+SourceFile_fullPath+tr("]. Error: [Unable to assemble video clips.]"));
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Failed");
        emit Send_progressbar_Add();
        return 0;//如果启用stop位,则直接return
    }
    //============================== 删除缓存文件 ====================================================
    if(ui->checkBox_KeepVideoCache->isChecked()==false)
    {
        QFile::remove(VideoConfiguration_fullPath);
        file_DelDir(SplitFramesFolderPath);
        file_DelDir(VideoClipsFolderPath);
        QFile::remove(AudioPath);
        if(SourceFile_fullPath!=video_mp4_fullpath)QFile::remove(video_mp4_fullpath);
    }
    else
    {
        DelOriginal=false;
    }
    //============================= 删除原文件 &  & 更新table status ============================
    if(DelOriginal)
    {
        if(ReplaceOriginalFile(SourceFile_fullPath,video_mp4_scaled_fullpath)==false)
        {
            if(QAction_checkBox_MoveToRecycleBin_checkBox_DelOriginal->isChecked())
            {
                file_MoveToTrash(SourceFile_fullPath);
            }
            else
            {
                QFile::remove(SourceFile_fullPath);
            }
        }
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Finished, original file deleted");
    }
    else
    {
        emit Send_Table_video_ChangeStatus_rowNumInt_statusQString(rowNum, "Finished");
    }
    //========== 移动到输出路径 =========
    if(ui->checkBox_OutPath_isEnabled->isChecked())
    {
        MoveFileToOutputPath(video_mp4_scaled_fullpath,SourceFile_fullPath);
    }
    //============================ 更新进度条 =================================
    emit Send_progressbar_Add();
    //===========================  ==============================
    return 0;
}
/*
================================================================================
                    WAIFU2X NCNN VULKAN 检测可用GPU
=================================================================================
*/
void MainWindow::on_pushButton_DetectGPU_clicked()
{
    //====
    ui->pushButton_DetectGPU->setText(tr("Detecting, please wait..."));
    //====
    pushButton_Start_setEnabled_self(0);
    ui->pushButton_DetectGPU->setEnabled(0);
    ui->pushButton_DetectGPUID_srmd->setEnabled(0);
    ui->pushButton_DumpProcessorList_converter->setEnabled(0);
    ui->pushButton_ListGPUs_Anime4k->setEnabled(0);
    ui->pushButton_compatibilityTest->setEnabled(0);
    ui->pushButton_DetectGPU_RealsrNCNNVulkan->setEnabled(0);
    Available_GPUID.clear();
    QtConcurrent::run(this, &MainWindow::Waifu2x_DetectGPU);
}

int MainWindow::Waifu2x_DetectGPU()
{
    emit Send_TextBrowser_NewMessage(tr("Detecting available GPU, please wait."));
    //===============
    QString InputPath = Current_Path + "/Compatibility_Test/Compatibility_Test.jpg";
    QString OutputPath = Current_Path + "/Compatibility_Test/res.png";
    QFile::remove(OutputPath);
    //==============
    QString Waifu2x_folder_path = Waifu2x_ncnn_vulkan_FolderPath;
    QString program = Waifu2x_ncnn_vulkan_ProgramPath;
    QString model_path = Waifu2x_folder_path+"/models-upconv_7_anime_style_art_rgb";
    //=========
    int GPU_ID=-1;
    //=========
    while(true)
    {
        QFile::remove(OutputPath);
        QProcess *Waifu2x = new QProcess();
        QString gpu_str = " -g "+QString::number(GPU_ID,10)+" ";
        QString cmd = "\"" + program + "\"" + " -i " + "\"" + InputPath + "\"" + " -o " + "\"" + OutputPath + "\"" + " -s 2 -n 0 -t 32 -m " + "\"" + model_path + "\"" + " -j 1:1:1"+gpu_str;
        Waifu2x->start(cmd);
        while(!Waifu2x->waitForStarted(100)&&!QProcess_stop) {}
        while(!Waifu2x->waitForFinished(100)&&!QProcess_stop) {}
        if(QFile::exists(OutputPath) && (Waifu2x->readAllStandardError().toLower().contains("failed")||Waifu2x->readAllStandardOutput().toLower().contains("failed"))==false)
        {
            Available_GPUID.append(QString::number(GPU_ID,10));
            GPU_ID++;
            QFile::remove(OutputPath);
        }
        else
        {
            if(GPU_ID > -1)
            {
                break;
            }
            else
            {
                GPU_ID++;
            }
        }
    }
    QFile::remove(OutputPath);
    //===============
    emit Send_TextBrowser_NewMessage(tr("Detection is complete!"));
    if(Available_GPUID.isEmpty())
    {
        Send_TextBrowser_NewMessage(tr("No available GPU ID detected!"));
    }
    emit Send_Waifu2x_DetectGPU_finished();
    return 0;
}

int MainWindow::Waifu2x_DetectGPU_finished()
{
    pushButton_Start_setEnabled_self(1);
    ui->pushButton_DetectGPU->setEnabled(1);
    ui->pushButton_compatibilityTest->setEnabled(1);
    ui->pushButton_DetectGPUID_srmd->setEnabled(1);
    ui->pushButton_DumpProcessorList_converter->setEnabled(1);
    on_checkBox_SpecifyGPU_Anime4k_stateChanged(1);
    ui->pushButton_DetectGPU_RealsrNCNNVulkan->setEnabled(1);
    //====
    GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.clear();
    ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->clear();
    ui->comboBox_GPUID->clear();
    ui->comboBox_GPUID->addItem("auto");
    if(!Available_GPUID.isEmpty())
    {
        for(int i=0; i<Available_GPUID.size(); i++)
        {
            ui->comboBox_GPUID->addItem(Available_GPUID.at(i));
            AddGPU_MultiGPU_Waifu2xNCNNVulkan(Available_GPUID.at(i));
        }
    }
    //====
    ui->pushButton_DetectGPU->setText(tr("Detect available GPU ID"));
    //====
    return 0;
}

/*
Waifu2x_NCNN_Vulkan
预读取引擎配置
*/
QString MainWindow::Waifu2x_NCNN_Vulkan_PreLoad_Settings()
{
    QString Waifu2x_NCNN_Vulkan_Settings_str = " ";
    //TTA
    if(ui->checkBox_TTA_vulkan->isChecked())
    {
        Waifu2x_NCNN_Vulkan_Settings_str.append("-x ");
    }
    if(ui->checkBox_MultiGPU_Waifu2xNCNNVulkan->isChecked()==false)
    {
        //==========单显卡==========
        //GPU ID
        if(ui->comboBox_GPUID->currentText()!="auto")
        {
            Waifu2x_NCNN_Vulkan_Settings_str.append("-g "+ui->comboBox_GPUID->currentText()+" ");
        }
        //Tile Size
        Waifu2x_NCNN_Vulkan_Settings_str.append("-t "+QString::number(ui->spinBox_TileSize->value(),10)+" ");
    }
    //Model
    int ImageStyle = ui->comboBox_ImageStyle->currentIndex();
    QString model_path="";
    if(ui->comboBox_model_vulkan->currentIndex()==0)
    {
        if(ImageStyle==0)
        {
            model_path = Waifu2x_ncnn_vulkan_FolderPath+"/models-upconv_7_anime_style_art_rgb";
        }
        else
        {
            model_path = Waifu2x_ncnn_vulkan_FolderPath+"/models-upconv_7_photo";
        }
    }
    if(ui->comboBox_model_vulkan->currentIndex()==1)
    {
        model_path = Waifu2x_ncnn_vulkan_FolderPath+"/models-cunet";
    }
    Waifu2x_NCNN_Vulkan_Settings_str.append("-m \""+model_path+"\" ");
    Waifu2x_NCNN_Vulkan_Settings_str.append("-j 1:1:1 ");
    //=======================================
    return Waifu2x_NCNN_Vulkan_Settings_str;
}

/*
Waifu2x_NCNN_Vulkan
读取配置生成配置string
*/
QString MainWindow::Waifu2x_NCNN_Vulkan_ReadSettings()
{
    QString Waifu2x_NCNN_Vulkan_Settings_str = "";
    Waifu2x_NCNN_Vulkan_Settings_str.append(Waifu2x_NCNN_Vulkan_PreLoad_Settings_Str);
    if(ui->checkBox_MultiGPU_Waifu2xNCNNVulkan->isChecked())
    {
        //==========多显卡==========
        QMap<QString,QString> GPUInfo = Waifu2x_NCNN_Vulkan_MultiGPU();
        //GPU ID
        Waifu2x_NCNN_Vulkan_Settings_str.append("-g "+GPUInfo["ID"]+" ");
        //Tile Size
        Waifu2x_NCNN_Vulkan_Settings_str.append("-t "+GPUInfo["TileSize"]+" ");
    }
    //=======================================
    return Waifu2x_NCNN_Vulkan_Settings_str;
}
/*
Waifu2x_NCNN_Vulkan
显卡切换
*/
QMap<QString,QString> MainWindow::Waifu2x_NCNN_Vulkan_MultiGPU()
{
    MultiGPU_QMutex_Waifu2xNCNNVulkan.lock();
    int MAX_GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU = GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.size()-1;
    if(GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU>MAX_GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU)
    {
        GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU=0;
    }
    //======
    QMap<QString,QString> GPUInfo;
    do
    {
        GPUInfo = GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.at(GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU);
        if(GPUInfo["isEnabled"] != "true")
        {
            GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU++;
            if(GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU>MAX_GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU)
            {
                GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU=0;
            }
        }
        else
        {
            break;
        }
    }
    while(true);
    //======
    GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU++;
    if(GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU>MAX_GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU)
    {
        GPU_ID_Waifu2x_NCNN_Vulkan_MultiGPU=0;
    }
    //======
    MultiGPU_QMutex_Waifu2xNCNNVulkan.unlock();
    return GPUInfo;
}

void MainWindow::on_checkBox_MultiGPU_Waifu2xNCNNVulkan_clicked()
{
    if(ui->checkBox_MultiGPU_Waifu2xNCNNVulkan->isChecked())
    {
        if(GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.size()==0)
        {
            QMessageBox *MSG = new QMessageBox();
            MSG->setWindowTitle(tr("Notification"));
            MSG->setText(tr("Please detect available GPU ID before enable Multi-GPU."));
            MSG->setIcon(QMessageBox::Information);
            MSG->setModal(true);
            MSG->show();
            ui->checkBox_MultiGPU_Waifu2xNCNNVulkan->setChecked(0);
            return;
        }
        if(GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.size()<2)
        {
            QMessageBox *MSG = new QMessageBox();
            MSG->setWindowTitle(tr("Error"));
            MSG->setText(tr("Insufficient number of available GPUs."));
            MSG->setIcon(QMessageBox::Warning);
            MSG->setModal(true);
            MSG->show();
            ui->checkBox_MultiGPU_Waifu2xNCNNVulkan->setChecked(0);
            return;
        }
        if(ui->comboBox_version_Waifu2xNCNNVulkan->currentIndex()!=0)
        {
            QMessageBox *MSG = new QMessageBox();
            MSG->setWindowTitle(tr("Warning"));
            MSG->setText(tr("Old versions of Waifu2x-NCNN-Vulkan doesn't support Multi-GPUs when processing Video and GIF."));
            MSG->setIcon(QMessageBox::Warning);
            MSG->setModal(true);
            MSG->show();
            return;
        }
    }
}

void MainWindow::AddGPU_MultiGPU_Waifu2xNCNNVulkan(QString GPUID)
{
    QMap<QString,QString> GPUInfo;
    GPUInfo["ID"] = GPUID;
    GPUInfo["isEnabled"] = "true";
    GPUInfo["TileSize"] = "100";
    GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.append(GPUInfo);
    ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->addItem(GPUID);
    ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->setCurrentIndex(0);
}

void MainWindow::on_comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan_currentIndexChanged(int index)
{
    if(ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->count()==0)
    {
        return;
    }
    QMap<QString,QString> GPUInfo=GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.at(ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->currentIndex());
    ui->checkBox_isEnable_CurrentGPU_MultiGPU_Waifu2xNCNNVulkan->setChecked(GPUInfo["isEnabled"] == "true");
    ui->spinBox_TileSize_CurrentGPU_MultiGPU_Waifu2xNCNNVulkan->setValue(GPUInfo["TileSize"].toInt());
}

void MainWindow::on_spinBox_TileSize_CurrentGPU_MultiGPU_Waifu2xNCNNVulkan_valueChanged(int arg1)
{
    QMap<QString,QString> GPUInfo=GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.at(ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->currentIndex());
    GPUInfo["TileSize"] = QString::number(ui->spinBox_TileSize_CurrentGPU_MultiGPU_Waifu2xNCNNVulkan->value(),10);
    GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.replace(ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->currentIndex(),GPUInfo);
}

void MainWindow::on_checkBox_isEnable_CurrentGPU_MultiGPU_Waifu2xNCNNVulkan_clicked()
{
    QMap<QString,QString> GPUInfo=GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.at(ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->currentIndex());
    if(ui->checkBox_isEnable_CurrentGPU_MultiGPU_Waifu2xNCNNVulkan->isChecked())
    {
        GPUInfo["isEnabled"] = "true";
    }
    else
    {
        GPUInfo["isEnabled"] = "false";
    }
    GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.replace(ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->currentIndex(),GPUInfo);
    int enabledGPUs = 0;
    for (int i=0; i<GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.size(); i++)
    {
        QMap<QString,QString> GPUInfo_tmp = GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.at(i);
        if(GPUInfo_tmp["isEnabled"] == "true")
        {
            enabledGPUs++;
        }
    }
    if(enabledGPUs<2)
    {
        QMap<QString,QString> GPUInfo=GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.at(ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->currentIndex());
        GPUInfo["isEnabled"] = "true";
        GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.replace(ui->comboBox_GPUIDs_MultiGPU_Waifu2xNCNNVulkan->currentIndex(),GPUInfo);
        ui->checkBox_isEnable_CurrentGPU_MultiGPU_Waifu2xNCNNVulkan->setChecked(1);
        QMessageBox *MSG = new QMessageBox();
        MSG->setWindowTitle(tr("Warning"));
        MSG->setText(tr("At least 2 GPUs need to be enabled !!"));
        MSG->setIcon(QMessageBox::Warning);
        MSG->setModal(true);
        MSG->show();
    }
}

void MainWindow::on_checkBox_MultiGPU_Waifu2xNCNNVulkan_stateChanged(int arg1)
{
    if(ui->checkBox_MultiGPU_Waifu2xNCNNVulkan->isChecked())
    {
        ui->comboBox_GPUID->setEnabled(0);
        ui->frame_TileSize_W2xNcnnVulkan->setEnabled(0);
        ui->groupBox_GPUSettings_MultiGPU_Waifu2xNCNNVulkan->setEnabled(1);
    }
    else
    {
        ui->comboBox_GPUID->setEnabled(1);
        ui->frame_TileSize_W2xNcnnVulkan->setEnabled(1);
        ui->groupBox_GPUSettings_MultiGPU_Waifu2xNCNNVulkan->setEnabled(0);
    }
}

void MainWindow::on_pushButton_ShowMultiGPUSettings_Waifu2xNCNNVulkan_clicked()
{
    int Max_GPU_ID = GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.size();
    QString MultiGPUSettings_str="";
    for(int GPU_ID=0; GPU_ID<Max_GPU_ID; GPU_ID++)
    {
        QMap<QString,QString> GPUInfo = GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.at(GPU_ID);
        if(GPUInfo["isEnabled"] != "true")
        {
            continue;
        }
        else
        {
            MultiGPUSettings_str.append("GPU ID:["+GPUInfo["ID"]+"] | "+tr("Tile size:")+"["+GPUInfo["TileSize"]+"]\n\n");
        }
    }
    //=========
    QMessageBox *MSG = new QMessageBox();
    MSG->setWindowTitle(tr("Current Multi-GPU settings"));
    MSG->setText(MultiGPUSettings_str);
    MSG->setIcon(QMessageBox::Information);
    MSG->setModal(true);
    MSG->show();
}
/*
W2x NCNN Vulkan 块大小调整按钮 +
*/
void MainWindow::on_pushButton_TileSize_Add_W2xNCNNVulkan_clicked()
{
    ui->spinBox_TileSize->setValue(AddTileSize_NCNNVulkan_Converter(ui->spinBox_TileSize->value()));
}
/*
W2x NCNN Vulkan 块大小调整按钮 -
*/
void MainWindow::on_pushButton_TileSize_Minus_W2xNCNNVulkan_clicked()
{
    ui->spinBox_TileSize->setValue(MinusTileSize_NCNNVulkan_Converter(ui->spinBox_TileSize->value()));
}
/*
计算 临时放大倍率
*/
int MainWindow::Calculate_Temporary_ScaleRatio_W2xNCNNVulkan(int ScaleRatio)
{
    int ScaleRatio_tmp=0;
    //如果设定的scaleRatio不是偶数,则+1,并输出到tmp
    if((ScaleRatio%2)==0)
    {
        ScaleRatio_tmp = ScaleRatio;
    }
    else
    {
        ScaleRatio_tmp = ScaleRatio+1;
    }
    //判断是否为2的幂数
    if((ScaleRatio_tmp&(ScaleRatio_tmp-1))!=0)
    {
        for(int i=1; true; i++)
        {
            int pow_ =pow(2,i);
            if(pow_>=ScaleRatio_tmp)
            {
                ScaleRatio_tmp=pow_;
                break;
            }
        }
    }
    return ScaleRatio_tmp;
}
/*
读取配置讯息 Video GIF
*/
QString MainWindow::Waifu2x_NCNN_Vulkan_ReadSettings_Video_GIF(int ThreadNum)
{
    QString Waifu2x_NCNN_Vulkan_Settings_str = " ";
    //TTA
    if(ui->checkBox_TTA_vulkan->isChecked())
    {
        Waifu2x_NCNN_Vulkan_Settings_str.append("-x ");
    }
    if(ui->checkBox_MultiGPU_Waifu2xNCNNVulkan->isChecked()==true && ui->comboBox_version_Waifu2xNCNNVulkan->currentIndex()==0)
    {
        //==============多显卡================
        int NumOfGPUs_Available = GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.size();//获取显卡总数
        int NumOfGPUs_Enabled = 0;
        QString GPU_ID_cmd = "-g ";
        QString TileSize_cmd = "-t ";
        for(int i=0; i<NumOfGPUs_Available; i++)
        {
            QMap<QString,QString> GPUInfo = GPUIDs_List_MultiGPU_Waifu2xNCNNVulkan.at(i);
            if(GPUInfo["isEnabled"] != "true")continue;
            NumOfGPUs_Enabled++;
            if(i==0)
            {
                GPU_ID_cmd.append(GPUInfo["ID"]);
                TileSize_cmd.append(GPUInfo["TileSize"]);
            }
            else
            {
                GPU_ID_cmd.append(","+GPUInfo["ID"]);
                TileSize_cmd.append(","+GPUInfo["TileSize"]);
            }
        }
        Waifu2x_NCNN_Vulkan_Settings_str.append(GPU_ID_cmd+" ");
        Waifu2x_NCNN_Vulkan_Settings_str.append(TileSize_cmd+" ");
        //线程数量
        int NumOfThreads_PerGPU = ThreadNum/NumOfGPUs_Enabled;
        if(NumOfThreads_PerGPU<1)NumOfThreads_PerGPU=1;
        int LoadAndWrite_tnum = NumOfGPUs_Enabled*NumOfThreads_PerGPU;
        QString Jobs_cmd = "";
        for(int i=0; i<NumOfGPUs_Enabled; i++)
        {
            if(i==0)
            {
                Jobs_cmd.append(QString("%1").arg(NumOfThreads_PerGPU));
            }
            else
            {
                Jobs_cmd.append(QString(",%1").arg(NumOfThreads_PerGPU));
            }
        }
        Waifu2x_NCNN_Vulkan_Settings_str.append(QString("-j %1:").arg(LoadAndWrite_tnum)+Jobs_cmd+QString(":%1 ").arg(LoadAndWrite_tnum));
    }
    else
    {
        //==========单显卡==========
        //GPU ID
        if(ui->comboBox_GPUID->currentText()!="auto" && ui->checkBox_MultiGPU_Waifu2xNCNNVulkan->isChecked()==false)
        {
            Waifu2x_NCNN_Vulkan_Settings_str.append("-g "+ui->comboBox_GPUID->currentText()+" ");
        }
        //Tile Size
        Waifu2x_NCNN_Vulkan_Settings_str.append("-t "+QString::number(ui->spinBox_TileSize->value(),10)+" ");
        //线程数量
        QString jobs_num_str = QString("%1").arg(ThreadNum);
        Waifu2x_NCNN_Vulkan_Settings_str.append("-j "+jobs_num_str+":"+jobs_num_str+":"+jobs_num_str+" ");
    }
    //Model
    int ImageStyle = ui->comboBox_ImageStyle->currentIndex();
    QString model_path="";
    if(ui->comboBox_model_vulkan->currentIndex()==0)
    {
        if(ImageStyle==0)
        {
            model_path = Waifu2x_ncnn_vulkan_FolderPath+"/models-upconv_7_anime_style_art_rgb";
        }
        else
        {
            model_path = Waifu2x_ncnn_vulkan_FolderPath+"/models-upconv_7_photo";
        }
    }
    if(ui->comboBox_model_vulkan->currentIndex()==1)
    {
        model_path = Waifu2x_ncnn_vulkan_FolderPath+"/models-cunet";
    }
    Waifu2x_NCNN_Vulkan_Settings_str.append("-m \""+model_path+"\" ");
    return Waifu2x_NCNN_Vulkan_Settings_str;
}
/*
计算 实际输入CMD的放大倍率 列表
*/
/*
QList<int> MainWindow::Calculate_CMD_ScaleRatio_List_W2xNCNNVulkan(int ScaleRatio)
{
    QList<int> CMD_ScaleRatio_List;
    int ScaleRatio_MAX = Calculate_Temporary_ScaleRatio_W2xNCNNVulkan(ScaleRatio);
    if(ScaleRatio_MAX<=32)
    {
        CMD_ScaleRatio_List.append(ScaleRatio_MAX);
        return CMD_ScaleRatio_List;
    }
    int ScaleRatio_MAX_tmp = ScaleRatio_MAX/32;
    CMD_ScaleRatio_List.append(32);
    do
    {
        if(ScaleRatio_MAX_tmp/32 > 1)
        {
            ScaleRatio_MAX_tmp/=32;
            CMD_ScaleRatio_List.append(32);
        }
        else
        {
            CMD_ScaleRatio_List.append(ScaleRatio_MAX_tmp);
            break;
        }
    }
    while(true);
    return CMD_ScaleRatio_List;
}
*/
