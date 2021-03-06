#	$NetBSD: Makefile,v 1.1 2019/11/11 22:45:14 joerg Exp $

LIB=	clangAST

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/AST

SRCS+=	APValue.cpp \
	ASTConsumer.cpp \
	ASTContext.cpp \
	ASTDiagnostic.cpp \
	ASTDumper.cpp \
	ASTImporter.cpp \
	ASTImporterLookupTable.cpp \
	ASTStructuralEquivalence.cpp \
	ASTTypeTraits.cpp \
	AttrImpl.cpp \
	CommentBriefParser.cpp \
	CommentCommandTraits.cpp \
	Comment.cpp \
	CommentLexer.cpp \
	CommentParser.cpp \
	CommentSema.cpp \
	ComparisonCategories.cpp \
	CXXInheritance.cpp \
	DataCollection.cpp \
	DeclarationName.cpp \
	DeclBase.cpp \
	Decl.cpp \
	DeclCXX.cpp \
	DeclFriend.cpp \
	DeclGroup.cpp \
	DeclObjC.cpp \
	DeclOpenMP.cpp \
	DeclPrinter.cpp \
	DeclTemplate.cpp \
	ExprClassification.cpp \
	ExprConstant.cpp \
	Expr.cpp \
	ExprCXX.cpp \
	ExprObjC.cpp \
	ExternalASTMerger.cpp \
	ExternalASTSource.cpp \
	FormatString.cpp \
	InheritViz.cpp \
	ItaniumCXXABI.cpp \
	ItaniumMangle.cpp \
	JSONNodeDumper.cpp \
	Mangle.cpp \
	MicrosoftCXXABI.cpp \
	MicrosoftMangle.cpp \
	NestedNameSpecifier.cpp \
	NSAPI.cpp \
	ODRHash.cpp \
	OpenMPClause.cpp \
	OSLog.cpp \
	ParentMap.cpp \
	PrintfFormatString.cpp \
	QualTypeNames.cpp \
	RawCommentList.cpp \
	RecordLayoutBuilder.cpp \
	RecordLayout.cpp \
	ScanfFormatString.cpp \
	SelectorLocationsKind.cpp \
	Stmt.cpp \
	StmtCXX.cpp \
	StmtIterator.cpp \
	StmtObjC.cpp \
	StmtOpenMP.cpp \
	StmtPrinter.cpp \
	StmtProfile.cpp \
	StmtViz.cpp \
	TemplateBase.cpp \
	TemplateName.cpp \
	TextNodeDumper.cpp \
	Type.cpp \
	TypeLoc.cpp \
	TypePrinter.cpp \
	VTableBuilder.cpp \
	VTTBuilder.cpp

.PATH: ${CLANG_SRCDIR}/lib/AST/Interp

SRCS+=	Block.cpp \
	ByteCodeEmitter.cpp \
	ByteCodeExprGen.cpp \
	ByteCodeGenError.cpp \
	ByteCodeStmtGen.cpp \
	Context.cpp \
	Descriptor.cpp \
	Disasm.cpp \
	EvalEmitter.cpp \
	Frame.cpp \
	Function.cpp \
	Interp.cpp \
	InterpFrame.cpp \
	InterpStack.cpp \
	InterpState.cpp \
	Pointer.cpp \
	PrimType.cpp \
	Program.cpp \
	Record.cpp \
	Source.cpp \
	State.cpp

CLANG_TABLEGEN_SRC=	Opcodes.td
CLANG_TABLEGEN_OUTPUT= \
	Opcodes.inc|-gen-clang-opcodes

.include "${.PARSEDIR}/../../tablegen.mk"

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
