// Title startup hook, derived from the generated PPC body.
// Replace fixed 1080p dimensions before surface allocation.
REX_HOOK_RAW(sub_82100D28) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x824767c0
	ctx.lr = 0x82100D30;
	__savegprlr_26(ctx, base);
	// stwu r1,-224(r1)
	ea = -224 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// li r10,6
	ctx.r10.s64 = 6;
	// li r30,0
	ctx.r30.s64 = 0;
	// addi r11,r1,104
	ctx.r11.s64 = ctx.r1.s64 + 104;
	// li r28,1
	ctx.r28.s64 = 1;
	// mr r9,r30
	ctx.r9.u64 = ctx.r30.u64;
	// mtctr r10
	ctx.ctr.u64 = ctx.r10.u64;
loc_82100D4C:
	// stdu r9,8(r11)
	ea = 8 + ctx.r11.u32;
	REX_STORE_U64(ea, ctx.r9.u64);
	ctx.r11.u32 = ea;
	// bdnz 0x82100d4c
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82100D4C;
	// addi r3,r1,112
	ctx.r3.s64 = ctx.r1.s64 + 112;
	// bl 0x8249914c
	ctx.lr = 0x82100D5C;
	__imp__XGetVideoMode(ctx, base);
	// lwz r11,120(r1)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r1.u32 + 120);
	// li r31,1920
	ctx.r31.s64 = 1920;
	// li r29,1080
	ctx.r29.s64 = 1080;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq cr6,0x82100d88
	if (ctx.cr6.eq) goto loc_82100D88;
	// lwz r11,116(r1)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r1.u32 + 116);
	// rlwinm r10,r11,0,0,30
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0xFFFFFFFE;
	// cmplwi cr6,r10,720
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 720, ctx.xer);
	// bge cr6,0x82100d88
	if (!ctx.cr6.lt) goto loc_82100D88;
	// li r31,1280
	ctx.r31.s64 = 1280;
	// li r29,720
	ctx.r29.s64 = 720;
loc_82100D88:
	ctx.r31.u64 = std::stoul(rex::cvar::GetFlagByName("video_mode_width"));
	ctx.r29.u64 = std::stoul(rex::cvar::GetFlagByName("video_mode_height"));
	REXLOG_INFO("VF2 native backbuffer requested: {}x{}", ctx.r31.u32, ctx.r29.u32);
	// lis r27,-32179
	ctx.r27.s64 = -2108882944;
	// li r5,124
	ctx.r5.s64 = 124;
	// addi r26,r27,-17216
	ctx.r26.s64 = ctx.r27.s64 + -17216;
	// li r4,0
	ctx.r4.s64 = 0;
	// mr r3,r26
	ctx.r3.u64 = ctx.r26.u64;
	// bl 0x824774d0
	ctx.lr = 0x82100DA0;
	sub_824774D0(ctx, base);
	// mr r9,r30
	ctx.r9.u64 = ctx.r30.u64;
	// lis r9,11680
	ctx.r9.s64 = 765460480;
	// stw r31,-17216(r27)
	REX_STORE_U32(ctx.r27.u32 + -17216, ctx.r31.u32);
	// mr r10,r30
	ctx.r10.u64 = ctx.r30.u64;
	// stw r29,4(r26)
	REX_STORE_U32(ctx.r26.u32 + 4, ctx.r29.u32);
	// li r10,1
	ctx.r10.s64 = 1;
	// stw r30,16(r26)
	REX_STORE_U32(ctx.r26.u32 + 16, ctx.r30.u32);
	// ori r9,r9,406
	ctx.r9.u64 = ctx.r9.u64 | 406;
	// stw r30,28(r26)
	REX_STORE_U32(ctx.r26.u32 + 28, ctx.r30.u32);
	// lis r11,6184
	ctx.r11.s64 = 405274624;
	// stw r10,24(r26)
	REX_STORE_U32(ctx.r26.u32 + 24, ctx.r10.u32);
	// stw r9,40(r26)
	REX_STORE_U32(ctx.r26.u32 + 40, ctx.r9.u32);
	// mr r10,r30
	ctx.r10.u64 = ctx.r30.u64;
	// ori r11,r11,390
	ctx.r11.u64 = ctx.r11.u64 | 390;
	// stw r30,12(r26)
	REX_STORE_U32(ctx.r26.u32 + 12, ctx.r30.u32);
	// li r10,60
	ctx.r10.s64 = 60;
	// stw r30,20(r26)
	REX_STORE_U32(ctx.r26.u32 + 20, ctx.r30.u32);
	// mr r9,r30
	ctx.r9.u64 = ctx.r30.u64;
	// stw r11,8(r26)
	REX_STORE_U32(ctx.r26.u32 + 8, ctx.r11.u32);
	// lis r9,10280
	ctx.r9.s64 = 673710080;
	// stw r10,48(r26)
	REX_STORE_U32(ctx.r26.u32 + 48, ctx.r10.u32);
	// mr r11,r30
	ctx.r11.u64 = ctx.r30.u64;
	// stw r30,32(r26)
	REX_STORE_U32(ctx.r26.u32 + 32, ctx.r30.u32);
	// li r11,1
	ctx.r11.s64 = 1;
	// stw r30,36(r26)
	REX_STORE_U32(ctx.r26.u32 + 36, ctx.r30.u32);
	// li r10,1
	ctx.r10.s64 = 1;
	// stw r30,44(r26)
	REX_STORE_U32(ctx.r26.u32 + 44, ctx.r30.u32);
	// ori r9,r9,262
	ctx.r9.u64 = ctx.r9.u64 | 262;
	// stw r11,56(r26)
	REX_STORE_U32(ctx.r26.u32 + 56, ctx.r11.u32);
	// stw r10,60(r26)
	REX_STORE_U32(ctx.r26.u32 + 60, ctx.r10.u32);
	// lis r10,-32179
	ctx.r10.s64 = -2108882944;
	// stw r30,52(r26)
	REX_STORE_U32(ctx.r26.u32 + 52, ctx.r30.u32);
	// mr r7,r26
	ctx.r7.u64 = ctx.r26.u64;
	// stw r9,64(r26)
	REX_STORE_U32(ctx.r26.u32 + 64, ctx.r9.u32);
	// addi r31,r10,-17092
	ctx.r31.s64 = ctx.r10.s64 + -17092;
	// stw r30,68(r26)
	REX_STORE_U32(ctx.r26.u32 + 68, ctx.r30.u32);
	// li r6,1
	ctx.r6.s64 = 1;
	// mr r8,r31
	ctx.r8.u64 = ctx.r31.u64;
	// li r5,0
	ctx.r5.s64 = 0;
	// li r4,1
	ctx.r4.s64 = 1;
	// li r3,0
	ctx.r3.s64 = 0;
	// mr r11,r30
	ctx.r11.u64 = ctx.r30.u64;
	// bl 0x821c3638
	ctx.lr = 0x82100E4C;
	sub_821C3638(ctx, base);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
	// bge cr6,0x82100e60
	if (!ctx.cr6.lt) goto loc_82100E60;
loc_82100E54:
	// li r3,0
	ctx.r3.s64 = 0;
	// addi r1,r1,224
	ctx.r1.s64 = ctx.r1.s64 + 224;
	// b 0x82476810
	__restgprlr_26(ctx, base);
	return;
loc_82100E60:
	// lis r11,-32179
	ctx.r11.s64 = -2108882944;
	// li r7,0
	ctx.r7.s64 = 0;
	// addi r8,r11,-17084
	ctx.r8.s64 = ctx.r11.s64 + -17084;
	// li r6,0
	ctx.r6.s64 = 0;
	// li r5,0
	ctx.r5.s64 = 0;
	// li r4,2
	ctx.r4.s64 = 2;
	// li r3,0
	ctx.r3.s64 = 0;
	// bl 0x821c3638
	ctx.lr = 0x82100E80;
	sub_821C3638(ctx, base);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
	// blt cr6,0x82100e54
	if (ctx.cr6.lt) goto loc_82100E54;
	// li r4,1
	ctx.r4.s64 = 1;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// bl 0x821b8178
	ctx.lr = 0x82100E94;
	sub_821B8178(ctx, base);
	// li r4,0
	ctx.r4.s64 = 0;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// bl 0x821b8188
	ctx.lr = 0x82100EA0;
	sub_821B8188(ctx, base);
	// li r4,1
	ctx.r4.s64 = 1;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// bl 0x821b7e90
	ctx.lr = 0x82100EAC;
	sub_821B7E90(ctx, base);
	// li r3,1
	ctx.r3.s64 = 1;
	// bl 0x821deb28
	ctx.lr = 0x82100EB4;
	sub_821DEB28(ctx, base);
	// bl 0x820fc230
	ctx.lr = 0x82100EB8;
	sub_820FC230(ctx, base);
	// bl 0x82100538
	ctx.lr = 0x82100EBC;
	sub_82100538(ctx, base);
	// clrlwi r11,r3,24
	ctx.r11.u64 = ctx.r3.u32 & 0xFF;
	// lis r10,-32240
	ctx.r10.s64 = -2112880640;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// subfic r9,r11,0
	ctx.xer.ca = ctx.r11.u32 <= 0;
	ctx.r9.u64 = static_cast<uint64_t>(0) - ctx.r11.u64;
	// addi r4,r10,576
	ctx.r4.s64 = ctx.r10.s64 + 576;
	// subfe r7,r8,r8
	temp.u8 = (~ctx.r8.u32 + ctx.r8.u32 < ~ctx.r8.u32) | (~ctx.r8.u32 + ctx.r8.u32 + ctx.xer.ca < ctx.xer.ca);
	ctx.r7.u64 = ~ctx.r8.u64 + ctx.r8.u64 + ctx.xer.ca;
	ctx.xer.ca = temp.u8;
	// and r29,r7,r28
	ctx.r29.u64 = ctx.r7.u64 & ctx.r28.u64;
	// bl 0x821cdce8
	ctx.lr = 0x82100EDC;
	sub_821CDCE8(ctx, base);
	// lis r6,-32240
	ctx.r6.s64 = -2112880640;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// addi r4,r6,672
	ctx.r4.s64 = ctx.r6.s64 + 672;
	// bl 0x821cdce0
	ctx.lr = 0x82100EEC;
	sub_821CDCE0(ctx, base);
	// lis r5,-32179
	ctx.r5.s64 = -2108882944;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// mr r11,r30
	ctx.r11.u64 = ctx.r30.u64;
	// addi r28,r5,-17056
	ctx.r28.s64 = ctx.r5.s64 + -17056;
	// mr r10,r30
	ctx.r10.u64 = ctx.r30.u64;
	// mr r9,r30
	ctx.r9.u64 = ctx.r30.u64;
	// mr r8,r30
	ctx.r8.u64 = ctx.r30.u64;
	// mr r7,r30
	ctx.r7.u64 = ctx.r30.u64;
	// stw r30,212(r28)
	REX_STORE_U32(ctx.r28.u32 + 212, ctx.r30.u32);
	// mr r6,r30
	ctx.r6.u64 = ctx.r30.u64;
	// stw r30,216(r28)
	REX_STORE_U32(ctx.r28.u32 + 216, ctx.r30.u32);
	// stw r30,220(r28)
	REX_STORE_U32(ctx.r28.u32 + 220, ctx.r30.u32);
	// stw r30,8(r28)
	REX_STORE_U32(ctx.r28.u32 + 8, ctx.r30.u32);
	// stw r30,224(r28)
	REX_STORE_U32(ctx.r28.u32 + 224, ctx.r30.u32);
	// stw r30,228(r28)
	REX_STORE_U32(ctx.r28.u32 + 228, ctx.r30.u32);
	// stw r30,232(r28)
	REX_STORE_U32(ctx.r28.u32 + 232, ctx.r30.u32);
	// lwz r27,4(r28)
	ctx.r27.u64 = REX_LOAD_U32(ctx.r28.u32 + 4);
	// bl 0x821ce1a8
	ctx.lr = 0x82100F34;
	sub_821CE1A8(ctx, base);
	// stb r30,241(r28)
	REX_STORE_U8(ctx.r28.u32 + 241, ctx.r30.u8);
	// lis r4,-32256
	ctx.r4.s64 = -2113929216;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// li r10,0
	ctx.r10.s64 = 0;
	// li r9,0
	ctx.r9.s64 = 0;
	// li r7,0
	ctx.r7.s64 = 0;
	// li r6,1
	ctx.r6.s64 = 1;
	// lfs f1,2248(r4)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r4.u32 + 2248);
	ctx.f1.f64 = double(temp.f32);
	// li r5,0
	ctx.r5.s64 = 0;
	// li r4,0
	ctx.r4.s64 = 0;
	// mr r11,r30
	ctx.r11.u64 = ctx.r30.u64;
	// bl 0x821c6330
	ctx.lr = 0x82100F64;
	sub_821C6330(ctx, base);
	// lis r11,-32256
	ctx.r11.s64 = -2113929216;
	// mr r6,r27
	ctx.r6.u64 = ctx.r27.u64;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// li r10,0
	ctx.r10.s64 = 0;
	// stw r30,100(r1)
	REX_STORE_U32(ctx.r1.u32 + 100, ctx.r30.u32);
	// li r9,0
	ctx.r9.s64 = 0;
	// stw r30,92(r1)
	REX_STORE_U32(ctx.r1.u32 + 92, ctx.r30.u32);
	// li r8,0
	ctx.r8.s64 = 0;
	// li r7,0
	ctx.r7.s64 = 0;
	// lfs f1,2244(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r11.u32 + 2244);
	ctx.f1.f64 = double(temp.f32);
	// li r5,0
	ctx.r5.s64 = 0;
	// li r4,0
	ctx.r4.s64 = 0;
	// bl 0x821c8a30
	ctx.lr = 0x82100F98;
	sub_821C8A30(ctx, base);
	// addis r11,r28,14
	ctx.r11.s64 = ctx.r28.s64 + 917504;
	// addi r3,r11,3404
	ctx.r3.s64 = ctx.r11.s64 + 3404;
	// bl 0x820fed68
	ctx.lr = 0x82100FA4;
	sub_820FED68(ctx, base);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r5,0
	ctx.r5.s64 = 0;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// li r4,0
	ctx.r4.s64 = 0;
	// bl 0x821bdd50
	ctx.lr = 0x82100FB8;
	sub_821BDD50(ctx, base);
	// li r5,0
	ctx.r5.s64 = 0;
	// mr r4,r27
	ctx.r4.u64 = ctx.r27.u64;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// bl 0x821ce1b0
	ctx.lr = 0x82100FC8;
	sub_821CE1B0(ctx, base);
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// li r6,80
	ctx.r6.s64 = 80;
	// li r5,48
	ctx.r5.s64 = 48;
	// li r4,0
	ctx.r4.s64 = 0;
	// bl 0x821bdd50
	ctx.lr = 0x82100FDC;
	sub_821BDD50(ctx, base);
	// addis r11,r28,14
	ctx.r11.s64 = ctx.r28.s64 + 917504;
	// addi r3,r11,3404
	ctx.r3.s64 = ctx.r11.s64 + 3404;
	// bl 0x820fed40
	ctx.lr = 0x82100FE8;
	sub_820FED40(ctx, base);
	// mr r3,r29
	ctx.r3.u64 = ctx.r29.u64;
	// addi r1,r1,224
	ctx.r1.s64 = ctx.r1.s64 + 224;
	// b 0x82476810
	__restgprlr_26(ctx, base);
	return;
}
