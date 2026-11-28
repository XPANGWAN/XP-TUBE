# XP-TUBE Platform | منصة XP-TUBE

## English

The XP Platform module defines backend-neutral lifecycle, controller, display, audio-focus, text-input, and dialog event contracts, together with a bounded event queue and an XP-owned runtime boundary.

The application currently routes the existing SDL event stream through XP Runtime/EventQueue. SDL remains the single owner of initialization and polling; XP does not initialize SDL again or perform a second poll. During migration, XP events are temporarily translated back to the inherited screen-event vocabulary.

XP currently owns the PS4 OpenOrbis system-service bootstrap, system language ID lookup, timezone query, and PS4 numeric-language-to-BCP-47 mapping. Controller discovery and polling are still performed by the inherited SDL platform layer. The next migration slice introduces XP-owned controller identity/lifecycle primitives without changing SDL ownership or duplicating hardware access.

Display/presentation, audio focus, and system dialogs remain in the migration layer. This is a staged migration, not a complete independent PS4 backend.

Host tests validate the backend-neutral contracts and state logic. They do not prove PS4 hardware behavior; a real-console test remains required.

## العربية

تعرّف وحدة XP Platform عقوداً مستقلة عن نوع المنصة لدورة التشغيل، ووحدة التحكم، والشاشة، وتركيز الصوت، وإدخال النص، وحوارات النظام، إضافة إلى طابور أحداث محدود وواجهة وقت تشغيل مملوكة لـ XP.

يمرّر التطبيق حالياً تدفق أحداث SDL الحالي عبر XP Runtime/EventQueue. تظل SDL المالك الوحيد للتهيئة وقراءة الأحداث؛ ولا تقوم XP بتهيئة SDL مرة أخرى أو بقراءة الأحداث للمرة الثانية. وخلال الترحيل، تُحوّل أحداث XP مؤقتاً إلى صيغة الأحداث التي تستخدمها الشاشات الموروثة.

تمتلك XP حالياً تهيئة خدمات نظام PS4 عبر OpenOrbis، وقراءة معرّف لغة النظام، والاستعلام عن المنطقة الزمنية، وتحويل معرّفات لغات PS4 الرقمية إلى BCP-47. ما زالت طبقة SDL الموروثة مسؤولة عن اكتشاف وحدات التحكم وقراءة أحداثها. وتضيف الخطوة التالية أدوات XP لإدارة هوية وحدات التحكم ودورة اتصالها دون تغيير ملكية SDL أو تكرار الوصول إلى العتاد.

ما زالت الشاشة والعرض وتركيز الصوت وحوارات النظام ضمن طبقة الترحيل. هذا ترحيل تدريجي وليس تنفيذاً مستقلاً كاملاً لمنصة PS4.

تتحقق اختبارات المضيف من العقود المستقلة ومنطق الحالة، لكنها لا تثبت عمل العتاد على PS4؛ لذلك يظل الاختبار على جهاز حقيقي مطلوباً.


## Controller lifecycle boundary | حدّ دورة اتصال وحدة التحكم

The XP-owned `ControllerRegistry` tracks up to eight controller instance IDs, rejects duplicate connections, ignores unknown disconnects, and reuses slots after disconnect. It deliberately does not open, close, or poll SDL/OpenOrbis devices; hardware ownership remains single-source until the adapter migration is complete.

تتولى `ControllerRegistry` المملوكة لـ XP تتبع ما يصل إلى ثماني هويات لوحدات التحكم، وترفض الاتصالات المكررة، وتتجاهل قطع الاتصال لهوية غير معروفة، وتعيد استخدام الخانات بعد قطع الاتصال. ولا تفتح أجهزة SDL/OpenOrbis أو تغلقها أو تقرأها؛ إذ تبقى ملكية العتاد لدى مصدر واحد إلى أن يكتمل ترحيل المحوّل.

## Display and presentation contract | عقد الشاشة والعرض

XP now defines a bounded, backend-neutral display contract in `display.hpp`. `DisplayInfo` describes the drawable dimensions, refresh rate, and display mode. `PresentationFrame` describes a non-owning RGBA frame with explicit row stride and byte size; validation rejects null pixels, unsupported dimensions, undersized rows, and incomplete buffers. The contract does not create an SDL window, GL context, or renderer. The transitional SDL adapter now exposes `displayInfo()` from the existing drawable size and window flags; it reports refresh rate as zero when the backend cannot provide a reliable value. SDL remains the sole owner of the window/context, and platform-specific Linux and PS4 presentation paths are migrated separately. The application now submits a non-owning `PresentationFrame` through `presentXpFrame()`; the XP contract validates pixel pointer, dimensions, row stride, and buffer length before dispatching to the existing Linux or PS4 renderer. The renderers retain their platform-specific GPU/shader implementations.

تعرّف XP الآن عقداً مستقلاً ومحدوداً للشاشة والعرض في `display.hpp`. يصف `DisplayInfo` أبعاد مساحة الرسم ومعدل التحديث ونمط العرض. ويصف `PresentationFrame` إطار RGBA دون امتلاك الذاكرة، مع تحديد طول الصف وحجم المخزن صراحةً؛ ويرفض التحقق المؤشرات الفارغة والأبعاد غير المدعومة والصفوف الأقصر من المطلوب والمخازن غير المكتملة. لا ينشئ هذا العقد نافذة SDL أو سياق OpenGL أو محرك رسم. يوفّر محوّل SDL الانتقالي الآن الدالة `displayInfo()` اعتماداً على أبعاد مساحة الرسم وأعلام النافذة الحالية؛ ويعيد معدل تحديث صفراً عندما لا توفّر المنصة قيمة موثوقة. تظل SDL المالك الوحيد للنافذة والسياق الرسومي، بينما يجري ترحيل مساري العرض الخاصين بـ Linux وPS4 كلٌّ على حدة. يرسل التطبيق الآن إطاراً غير مالك للذاكرة من نوع `PresentationFrame` عبر `presentXpFrame()`؛ ويتحقق عقد XP من مؤشر البكسلات والأبعاد وطول الصف وحجم المخزن قبل تمريره إلى محرك الرسم الحالي في Linux أو PS4. وتظل تطبيقات GPU والمظللات الخاصة بكل منصة منفصلة كما هي.
