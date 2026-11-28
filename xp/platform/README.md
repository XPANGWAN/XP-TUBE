# XP-TUBE Platform | منصة XP-TUBE

## English

XP Platform provides backend-neutral contracts and tested state logic for runtime events, controllers, display/presentation, text input, and audio-focus policy. The bounded event queue and runtime boundary are XP-owned. A contract or policy is not considered an active platform integration until a real backend source drives it.

The application routes the existing SDL event stream through XP Runtime/EventQueue. SDL remains the single owner of initialization and polling; XP does not initialize SDL again or perform a second poll. During migration, supported XP events are temporarily translated back to the inherited screen-event vocabulary.

XP owns the PS4 OpenOrbis system-service bootstrap, system language ID lookup, timezone query, and PS4 numeric-language-to-BCP-47 mapping. XP also owns bounded controller identity/input state and the display/presentation validation contracts. SDL still owns the actual controller device handles, polling, window, and GL context. The Linux and PS4 renderers remain platform-specific inherited implementations behind the XP presentation boundary.

### Audio focus status

`AudioFocusPolicy` separates temporary platform focus from the user's explicit pause choice. It ensures that regaining focus does not resume playback when the user had paused it. Its state logic is host-tested, but it is not yet connected to a PS4 or SDL focus/suspend notification source, and it does not acquire or control the audio device. PS4 playback currently uses libmpv with its SDL audio output; actual focus-loss handling remains future integration work.

### Text input and dialogs

The PS4 text-input adapter uses OpenOrbis IME and Common Dialog APIs to display the system keyboard and report completion, cancellation, or failure. XP currently carries text-input events, but it does not yet own a general system-dialog service or the PS4 IME lifecycle.

Host tests validate backend-neutral contracts and state logic. They do not prove PS4 hardware behavior; a real-console test remains required. This is a staged migration, not a complete independent PS4 backend.

## العربية

توفر وحدة XP Platform عقوداً مستقلة عن المنصة ومنطق حالات مختبراً لأحداث وقت التشغيل، ووحدات التحكم، والشاشة والعرض، وإدخال النص، وسياسة تركيز الصوت. كما تمتلك XP طابور الأحداث المحدود وحدود وقت التشغيل. ولا نعدّ أي عقد أو سياسة تكاملاً فعلياً مع المنصة حتى يربطه مصدر حقيقي من طبقة النظام.

يمرّر التطبيق تدفق أحداث SDL الحالي عبر XP Runtime/EventQueue. تظل SDL المالك الوحيد للتهيئة وقراءة الأحداث؛ ولا تقوم XP بتهيئة SDL مرة أخرى أو بقراءة الأحداث للمرة الثانية. وخلال الترحيل، تُحوّل أحداث XP المدعومة مؤقتاً إلى صيغة الأحداث التي تستخدمها الشاشات الموروثة.

تمتلك XP تهيئة خدمات نظام PS4 عبر OpenOrbis، وقراءة معرّف لغة النظام، والاستعلام عن المنطقة الزمنية، وتحويل معرّفات لغات PS4 الرقمية إلى BCP-47. كما تمتلك XP حالة محدودة لهوية وحدات التحكم ومدخلاتها، وعقود التحقق من الشاشة وإطارات العرض. لكن SDL ما زالت تملك مقابض أجهزة التحكم الفعلية وقراءة أحداثها والنافذة وسياق OpenGL. وما زالت محركات الرسم في Linux وPS4 تطبيقات موروثة منفصلة خلف حد العرض الخاص بـ XP.

### حالة تركيز الصوت

تفصل `AudioFocusPolicy` بين فقدان تركيز المنصة مؤقتاً وبين اختيار المستخدم إيقاف التشغيل يدوياً. وتضمن ألا تؤدي استعادة التركيز إلى استئناف التشغيل إذا كان المستخدم قد أوقفه بنفسه. تم اختبار منطق الحالة على المضيف، لكنه لم يُربط بعد بمصدر إشعارات فقدان التركيز أو تعليق التطبيق في PS4 أو SDL، كما أنه لا يفتح جهاز الصوت ولا يتحكم فيه. يستخدم تشغيل PS4 حالياً libmpv مع مخرج الصوت SDL؛ وما زال الربط الفعلي لأحداث فقدان التركيز عملاً لاحقاً.

### إدخال النص وحوارات النظام

يستخدم محوّل إدخال النص في PS4 واجهتي OpenOrbis IME وCommon Dialog لعرض لوحة مفاتيح النظام والإبلاغ عن الاكتمال أو الإلغاء أو الفشل. تحمل XP حالياً أحداث إدخال النص، لكنها لا تمتلك بعد خدمة عامة مستقلة لحوارات النظام ولا دورة حياة IME الخاصة بـ PS4.

تتحقق اختبارات المضيف من العقود المستقلة ومنطق الحالة، لكنها لا تثبت عمل العتاد على PS4؛ لذلك يظل الاختبار على جهاز حقيقي مطلوباً. هذا ترحيل تدريجي وليس تنفيذاً مستقلاً كاملاً لمنصة PS4.

## Controller lifecycle boundary | حدّ دورة اتصال وحدة التحكم

The XP-owned `ControllerRegistry` tracks up to eight controller instance IDs, rejects duplicate connections, ignores unknown disconnects, and reuses slots after disconnect. `ControllerInputTranslator` keeps bounded per-device input state and applies trigger hysteresis. Neither opens, closes, or polls hardware; SDL remains the single hardware owner.

تتولى `ControllerRegistry` المملوكة لـ XP تتبع ما يصل إلى ثماني هويات لوحدات التحكم، وترفض الاتصالات المكررة، وتتجاهل قطع الاتصال لهوية غير معروفة، وتعيد استخدام الخانات بعد قطع الاتصال. ويحتفظ `ControllerInputTranslator` بحالة محدودة لمدخلات كل جهاز ويطبق آلية hysteresis للمحفزات. ولا يفتح أي منهما العتاد أو يغلقه أو يقرأه؛ إذ تظل SDL المالك الوحيد للعتاد.

## Display and presentation contract | عقد الشاشة والعرض

XP defines a bounded, backend-neutral display contract in `display.hpp`. `DisplayInfo` describes drawable dimensions, refresh rate, and display mode. `PresentationFrame` describes a non-owning RGBA frame with explicit row stride and byte size; validation rejects null pixels, unsupported dimensions, undersized rows, and incomplete buffers. The contract does not create an SDL window, GL context, or renderer. The SDL adapter exposes `displayInfo()` from the existing drawable size and window flags; it reports refresh rate as zero when the backend cannot provide a reliable value. The application submits a non-owning `PresentationFrame` through `presentXpFrame()`; XP validates the frame before dispatching it to the existing Linux or PS4 renderer. SDL remains the sole owner of the window/context, and the renderers retain their platform-specific GPU/shader implementations.

تعرّف XP عقداً مستقلاً ومحدوداً للشاشة والعرض في `display.hpp`. يصف `DisplayInfo` أبعاد مساحة الرسم ومعدل التحديث ونمط العرض. ويصف `PresentationFrame` إطار RGBA دون امتلاك الذاكرة، مع تحديد طول الصف وحجم المخزن صراحةً؛ ويرفض التحقق المؤشرات الفارغة والأبعاد غير المدعومة والصفوف الأقصر من المطلوب والمخازن غير المكتملة. لا ينشئ هذا العقد نافذة SDL أو سياق OpenGL أو محرك رسم. يوفّر محوّل SDL الدالة `displayInfo()` اعتماداً على أبعاد مساحة الرسم وأعلام النافذة الحالية؛ ويعيد معدل تحديث صفراً عندما لا توفّر المنصة قيمة موثوقة. يرسل التطبيق إطاراً غير مالك للذاكرة من نوع `PresentationFrame` عبر `presentXpFrame()`؛ ويتحقق XP من الإطار قبل تمريره إلى محرك الرسم الحالي في Linux أو PS4. تظل SDL المالك الوحيد للنافذة والسياق الرسومي، وتحتفظ محركات الرسم بتطبيقات GPU والمظللات الخاصة بكل منصة.
